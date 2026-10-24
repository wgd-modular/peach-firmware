"use strict";

const REPO = "wgd-modular/peach-firmware";
// The XIAO RA4M1's DFU bootloader. It keeps the first 16 KB of the 256 KB
// code flash for itself and writes the firmware right behind it.
const BOOTLOADER = { vendorId: 0x2886, productId: 0x8049 };
const FLASH_SIZE = (256 - 16) * 1024;
const ACCENTS = ["peach", "leaf", "apricot", "rose"];

let dfuDevice = null;

const $ = (sel, el) => (el || document).querySelector(sel);

/* ---------- release loading ---------- */

async function loadRelease() {
  const res = await fetch(`https://api.github.com/repos/${REPO}/releases/latest`);
  if (!res.ok) throw new Error(`GitHub API: ${res.status}`);
  return res.json();
}

function firmwareName(assetName) {
  // arduino-cli names a build after its sketch, "name.ino.bin". The release
  // workflow drops the ".ino", but accept both.
  return assetName.replace(/(\.ino)?\.bin$/, "");
}

async function fetchReadme(fw, tag) {
  for (const ref of [tag, "main"]) {
    const res = await fetch(`https://raw.githubusercontent.com/${REPO}/${ref}/src/${fw}/README.md`);
    if (res.ok) return res.text();
  }
  return null;
}

// The description is the first prose paragraph of a firmware's README, the
// controls are the table or bullet list of its "Controls" section.

function isBulletRow(line) {
  return /^[-*]\s+.+:\s*.+$/.test(line);
}

function bulletRunAt(lines, idx) {
  let start = idx;
  let end = idx + 1;
  while (start > 0 && isBulletRow(lines[start - 1].trim())) start--;
  while (end < lines.length && isBulletRow(lines[end].trim())) end++;
  const rows = [];
  for (let i = start; i < end; i++) {
    const m = lines[i].trim().match(/^[-*]\s+(.+?):\s*(.+)$/);
    if (m) rows.push([m[1], m[2]]);
  }
  return rows;
}

function parseReadme(md) {
  const lines = md.split("\n");

  // Description: the first prose paragraph, skipping the title, the
  // "Author"/"Version History" sections and anything that isn't prose.
  let description = "";
  let skip = false;
  for (const raw of lines) {
    const l = raw.trim();
    const heading = l.match(/^#{1,6}\s*(.+?)\s*$/);
    if (heading) {
      if (description) break;
      skip = /^(author|version history)\b/i.test(heading[1]);
      continue;
    }
    if (!l) {
      if (description) break;
      continue;
    }
    if (skip) continue;
    if (l.startsWith("|") || l.startsWith(">") || l.startsWith("```") || /^[-*]\s/.test(l) || /^\d+\.\s/.test(l)) {
      break;
    }
    description = description ? `${description} ${l}` : l;
  }

  // Controls: prefer a "Controls" section (table or bullet list), else the
  // first control-like bullet list anywhere in the document.
  let rows = [];
  let fromTable = false;
  const headingIdx = lines.findIndex(l => /^#{1,6}\s*controls?\s*:?\s*$/i.test(l.trim()));
  if (headingIdx >= 0) {
    let end = lines.length;
    for (let i = headingIdx + 1; i < lines.length; i++) {
      if (/^#{1,6}\s/.test(lines[i])) { end = i; break; }
    }
    for (let i = headingIdx + 1; i < end; i++) {
      const l = lines[i].trim();
      if (!l.startsWith("|")) continue;
      const cells = l.split("|").slice(1, -1).map(c => c.trim());
      if (cells.every(c => /^[-: ]*$/.test(c))) continue;
      rows.push(cells);
    }
    fromTable = rows.length > 0;
    if (!rows.length) {
      const bulletOffset = lines.slice(headingIdx + 1, end).findIndex(l => isBulletRow(l.trim()));
      if (bulletOffset >= 0) rows = bulletRunAt(lines, headingIdx + 1 + bulletOffset);
    }
  }
  if (!rows.length) {
    const idx = lines.findIndex(l => /^[-*]\s*(Pot|CV)\s*\d/i.test(l.trim()));
    if (idx >= 0) rows = bulletRunAt(lines, idx);
  }
  if (rows.length && !fromTable) {
    rows.unshift(["Control", "Function"]);
  }

  // The card has no room for the XIAO pin a control is wired to.
  const pin = fromTable ? rows[0].findIndex(c => /^pin$/i.test(c)) : -1;
  if (pin >= 0) rows = rows.map(r => r.filter((_, i) => i !== pin));

  return { description, rows };
}

// A description or a control can carry a markdown link, bold text or code;
// render those instead of leaving the raw markdown in the text.

function escapeHtml(s) {
  return s.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;").replace(/"/g, "&quot;");
}

function resolveReadmeLink(url, folder, tag) {
  if (/^([a-z][a-z0-9+.-]*:)?\/\//i.test(url) || url.startsWith("mailto:")) return url;
  if (url.startsWith("#")) return `https://github.com/${REPO}/blob/${tag}/src/${folder}/README.md${url}`;
  const parts = `src/${folder}/${url}`.split("/");
  const resolved = [];
  for (const part of parts) {
    if (part === "." || part === "") continue;
    if (part === "..") resolved.pop();
    else resolved.push(part);
  }
  return `https://github.com/${REPO}/blob/${tag}/${resolved.join("/")}`;
}

function renderEmphasis(html) {
  return html
    .replace(/\*\*(.+?)\*\*/g, "<strong>$1</strong>")
    .replace(/`([^`]+)`/g, "<code>$1</code>");
}

function renderInline(text, folder, tag) {
  const re = /\[([^\]]+)\]\(([^)]+)\)/g;
  let out = "";
  let last = 0;
  let m;
  while ((m = re.exec(text))) {
    out += renderEmphasis(escapeHtml(text.slice(last, m.index)));
    const href = escapeHtml(resolveReadmeLink(m[2], folder, tag));
    out += `<a href="${href}" target="_blank" rel="noopener">${renderEmphasis(escapeHtml(m[1]))}</a>`;
    last = re.lastIndex;
  }
  out += renderEmphasis(escapeHtml(text.slice(last)));
  return out;
}

/* ---------- cards ---------- */

function buildCard(asset, tag, accent) {
  const fw = firmwareName(asset.name);
  const card = document.createElement("article");
  card.className = `card ${accent}`;
  card.innerHTML = `
    <div class="card-head">
      <svg class="fw-mark" viewBox="0 0 48 48" aria-hidden="true"><use href="pattern.svg#peach" fill="currentColor"/></svg>
      <h2>${fw.replace(/-/g, " ")}</h2>
      <span class="ver">${tag}</span>
    </div>
    <div class="card-body">
      <p class="desc">Loading description…</p>
      <div class="controls" hidden><h3>Controls</h3><table></table></div>
      <p class="status-line" hidden></p>
      <div class="progress" hidden><div class="bar"></div><span class="ptext"></span></div>
      <div class="card-foot">
        <a class="readme-link" target="_blank" rel="noopener"
           href="https://github.com/${REPO}/blob/${tag}/src/${fw}/README.md">Full guide ↗</a>
        <button class="btn flash-btn">Flash</button>
      </div>
    </div>`;

  $(".flash-btn", card).addEventListener("click", () => flashAsset(asset, tag, card));

  fetchReadme(fw, tag).then(md => {
    if (!md) {
      $(".desc", card).textContent = "No description available.";
      return;
    }
    const { description, rows } = parseReadme(md);
    const desc = $(".desc", card);
    if (description) desc.innerHTML = renderInline(description, fw, tag);
    else desc.textContent = "No description available.";
    if (rows.length) {
      const table = $(".controls table", card);
      const [head, ...body] = rows;
      const cell = c => renderInline(c, fw, tag);
      const tr = h => `<tr>${h}</tr>`;
      table.innerHTML =
        tr(head.map(c => `<th>${cell(c)}</th>`).join("")) +
        body.map(r => tr(r.map(c => `<td>${cell(c)}</td>`).join(""))).join("");
      $(".controls", card).hidden = false;
    }
  });

  return card;
}

async function render() {
  if (!("usb" in navigator)) {
    $("#unsupported").hidden = false;
    $("#connect").disabled = true;
  }
  try {
    const release = await loadRelease();
    const date = new Date(release.published_at).toLocaleDateString("en-GB",
      { day: "numeric", month: "long", year: "numeric" });
    $("#release-chip").textContent = `latest release ${release.tag_name} · ${date}`;

    const manual = release.assets.find(a => a.name === "peach-manual.pdf");
    if (manual) {
      $("#manual a").href = manual.browser_download_url;
      $("#manual").hidden = false;
    }

    const bins = release.assets
      .filter(a => a.name.endsWith(".bin"))
      .sort((a, b) => a.name.localeCompare(b.name));

    const cards = $("#cards");
    bins.forEach((asset, i) =>
      cards.appendChild(buildCard(asset, release.tag_name, ACCENTS[i % ACCENTS.length])));
  } catch (err) {
    $("#release-chip").textContent = "release list unavailable";
    const box = $("#load-error");
    box.textContent = `Couldn’t load the release list from GitHub (${err.message}). ` +
      "You can still flash a local .bin below.";
    box.hidden = false;
  }
}

/* ---------- DFU ---------- */

// The bootloader is plain DFU 1.1, not ST's DfuSe: it takes the image block
// by block and only commits the last one when the download is manifested.
async function getFunctionalDescriptor(device) {
  try {
    const data = await device.readConfigurationDescriptor(0);
    const config = dfu.parseConfigurationDescriptor(data);
    for (const desc of config.descriptors) {
      if (desc.bDescriptorType === 0x21 && desc.hasOwnProperty("wTransferSize")) {
        return desc;
      }
    }
  } catch (e) { /* fall through */ }
  return { wTransferSize: 64, bmAttributes: 0x07 };
}

async function connectDevice() {
  const usbDevice = await navigator.usb.requestDevice({ filters: [BOOTLOADER] });

  let interfaces = dfu.findDeviceDfuInterfaces(usbDevice);
  if (!interfaces.length) throw new Error("no DFU interface — is the XIAO in bootloader mode?");

  await usbDevice.open();
  const probe = new dfu.Device(usbDevice, interfaces[0]);
  const names = await probe.readInterfaceNames();
  for (const intf of interfaces) {
    if (intf.name === null) {
      const c = intf.configuration.configurationValue;
      intf.name = names[c]?.[intf.interface.interfaceNumber]?.[intf.alternate.alternateSetting];
    }
  }

  // Alternate setting 0 is the code flash, 1 the data flash (EEPROM).
  const chosen = interfaces.find(i => i.name && i.name.includes("CodeFlash")) ||
    interfaces.find(i => i.alternate.alternateSetting === 0) || interfaces[0];
  const device = new dfu.Device(usbDevice, chosen);
  await device.open();
  return device;
}

async function ensureDevice() {
  if (dfuDevice && dfuDevice.device_.opened) return dfuDevice;
  dfuDevice = await connectDevice();
  const status = $("#device-status");
  status.textContent = `connected: ${dfuDevice.device_.productName || "XIAO RA4M1 bootloader"}`;
  status.classList.add("ok");
  navigator.usb.addEventListener("disconnect", e => {
    if (dfuDevice && e.device === dfuDevice.device_) {
      dfuDevice = null;
      status.textContent = "device disconnected";
      status.classList.remove("ok");
    }
  });
  return dfuDevice;
}

async function flashBuffer(buffer, ui) {
  if (buffer.byteLength > FLASH_SIZE) {
    throw new Error("file is larger than the XIAO's 240 KB of firmware flash");
  }

  const device = await ensureDevice();

  ui.status("Preparing…");
  let state = await device.getStatus();
  if (state.state === dfu.dfuERROR) await device.clearStatus();

  device.logProgress = (done, total) => {
    if (total) ui.progress(done / total, `${Math.round(done / total * 100)}%`);
  };
  device.logInfo = msg => {
    if (/copying/i.test(msg)) ui.status("Writing…");
    else if (/manifest/i.test(msg)) ui.status("Finishing…");
  };
  device.logWarning = () => {};
  device.logDebug = () => {};
  device.logError = () => {};

  const functional = await getFunctionalDescriptor(device);
  const manifestationTolerant = (functional.bmAttributes & 0x04) !== 0;
  await device.do_download(functional.wTransferSize, buffer, manifestationTolerant);
  dfuDevice = null;
  ui.progress(1, "100%");
  ui.done("Flashed! The module restarts on its own — if not, tap RESET.");
}

function cardUi(card) {
  const line = $(".status-line", card);
  const prog = $(".progress", card);
  const btn = $(".flash-btn", card) || $("#local-flash");
  return {
    start() { btn.disabled = true; line.hidden = false; line.className = "status-line"; prog.hidden = false; },
    status(t) { line.textContent = t; },
    progress(f, t) { prog.style.setProperty("--p", `${f * 100}%`); $(".ptext", prog).textContent = t; },
    done(t) { line.textContent = t; line.className = "status-line ok"; btn.disabled = false; },
    fail(t) { line.textContent = t; line.className = "status-line err"; prog.hidden = true; btn.disabled = false; }
  };
}

async function flashAsset(asset, tag, card) {
  const ui = cardUi(card);
  ui.start();
  try {
    ui.status("Downloading firmware…");
    // GitHub's release-asset download redirects to a storage host that sends
    // no Access-Control-Allow-Origin header, so a cross-origin fetch() of
    // asset.browser_download_url always fails CORS. The release workflow
    // also mirrors each .bin under firmwares/<tag>/ on this same origin.
    const res = await fetch(`firmwares/${tag}/${asset.name}`);
    if (!res.ok) throw new Error(`download failed (${res.status})`);
    const buffer = await res.arrayBuffer();
    await flashBuffer(buffer, ui);
  } catch (err) {
    ui.fail(friendly(err));
  }
}

function friendly(err) {
  const msg = err && err.message ? err.message : String(err);
  if (/No device selected/i.test(msg)) return "No device selected.";
  if (/no DFU interface|bootloader/i.test(msg)) return msg;
  return `Something went wrong: ${msg}`;
}

/* ---------- wiring ---------- */

$("#connect").addEventListener("click", async () => {
  try { await ensureDevice(); }
  catch (err) {
    const status = $("#device-status");
    status.textContent = friendly(err);
    status.classList.remove("ok");
  }
});

let localBuffer = null;
$("#local-file").addEventListener("change", async e => {
  const file = e.target.files[0];
  if (!file) return;
  localBuffer = await file.arrayBuffer();
  $("#local-name").textContent = `${file.name} · ${(file.size / 1024).toFixed(1)} KB`;
  $("#local-flash").disabled = false;
});

$("#local-flash").addEventListener("click", async () => {
  const prog = $("#local-progress");
  const line = document.createElement("p");
  const holder = $(".local-body");
  let ui = {
    start() { $("#local-flash").disabled = true; prog.hidden = false; },
    status(t) { $(".ptext", prog).textContent = t; },
    progress(f, t) { prog.style.setProperty("--p", `${f * 100}%`); $(".ptext", prog).textContent = t; },
    done(t) { $(".ptext", prog).textContent = "done"; line.className = "status-line ok"; line.textContent = t; holder.appendChild(line); $("#local-flash").disabled = false; },
    fail(t) { line.className = "status-line err"; line.textContent = t; holder.appendChild(line); $("#local-flash").disabled = false; }
  };
  ui.start();
  try { await flashBuffer(localBuffer, ui); }
  catch (err) { ui.fail(friendly(err)); }
});

render();
