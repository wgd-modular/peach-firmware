// The Peach manual. The general chapters are written here, the
// firmware chapters come from the README of every firmware in src/,
// collected into build/data.json by build.py.

#import "@preview/cmarker:0.1.10"

#let data = json("build/data.json")

#let peach = rgb("#ee8a5e")
#let peach-deep = rgb("#c25a32")
#let leaf = rgb("#5e8c4a")
#let leaf-deep = rgb("#436a33")
#let rose = rgb("#d2596b")
#let rose-deep = rgb("#a83e50")
#let apricot = rgb("#e2a23b")
#let apricot-deep = rgb("#a8701a")
#let blossom = rgb("#f5b38e")
#let cream = rgb("#fff1e6")
#let cream-soft = rgb("#fffaf6")
#let ink = rgb("#3d2418")
#let ink-soft = rgb("#7a5546")
#let accents = (peach, leaf, apricot, rose)
#let deep-of = (
  (peach.to-hex()): peach-deep,
  (leaf.to-hex()): leaf-deep,
  (rose.to-hex()): rose-deep,
  (apricot.to-hex()): apricot-deep,
)
#let deep(colour) = deep-of.at(colour.to-hex(), default: colour)

#let accent = state("accent", peach)
#let serif = "Fraunces"
// The variable font registers itself with its default optical size.
#let sans = "Nunito Sans 12pt"

#let peaches(size: 21mm) = tiling(size: (size, size), relative: "self",
  image("assets/pattern.svg", width: size, height: size))

// The wave from the flasher's hero, tiled along the edge of the tiles.
#let band-width = 12mm
#let band-height = band-width * 26 / 88
#let band-tip = band-height / 2
#let band-line = band-height / 2
#let band(width: 148mm) = box(width: width, height: band-height, clip: true,
  rect(width: 100%, height: 100%, stroke: none,
    fill: tiling(size: (band-width, band-height),
      image("assets/wave.svg", width: band-width, height: band-height))))

// Tiles from the top of the page down to `bottom`, ending on a whole row.
// The wave covers that edge.
#let tiled-head(bottom, size: 21mm, width: 148mm) = {
  let rows = calc.ceil(bottom / size)
  place(top + left, dy: bottom - rows * size,
    rect(width: width, height: rows * size, fill: peaches(size: size)))
  place(top + left, dy: bottom - band-tip, band(width: width))
}

#let eyebrow = state("eyebrow", none)

#let diamond(fill: blossom, size: 4pt) = box(rotate(45deg,
  rect(width: size, height: size, fill: fill)))

#let month(date) = {
  let (y, m, d) = date.split("-").map(int)
  datetime(year: y, month: m, day: d).display(
    "[day padding:none] [month repr:long] [year]")
}

#let markdown(text, base: none) = cmarker.render(text, scope: (
  image: (path, alt: none) => image(
    if base == none { path } else { "/" + base + "/" + path }, alt: alt),
  link: (dest, body) => if type(dest) == label {
    body
  } else if dest.starts-with("chapter:") {
    std.link(label(dest.slice(8)), body)
  } else {
    std.link(dest, body)
  },
))

// ---------------------------------------------------------------- styles

#set document(title: "Peach manual", author: "wgd modular")
#set text(font: sans, size: 9pt, fill: ink, lang: "en")
#set par(justify: false, leading: 0.66em, spacing: 0.95em)
#set page(
  paper: "a5",
  margin: (x: 15mm, top: 19mm, bottom: 15mm),
  fill: cream-soft,
  header: context {
    let page-no = here().page()
    let openers = query(heading.where(level: 1))
    if openers.any(h => h.location().page() == page-no) { return }
    let before = query(heading.where(level: 1).before(here()))
    set text(size: 7pt, fill: ink-soft)
    grid(columns: (1fr, auto), align: (left, right),
      text(font: serif, style: "italic",
        if before.len() > 0 { before.last().body }),
      text(font: serif, style: "italic", fill: peach-deep)[Peach])
    v(-1.6mm)
    line(length: 100%, stroke: 0.4pt + blossom)
  },
  footer: context align(center, box(width: 7mm, height: 7mm,
    align(center + horizon, {
      place(center + horizon, rotate(45deg,
        rect(width: 4.6mm, height: 4.6mm, fill: blossom, radius: 0.4mm)))
      text(size: 7pt, weight: 700, fill: ink, counter(page).display())
    }))),
)

#show link: set text(fill: peach-deep)
#set strong(delta: 200)
#show raw: set text(font: "DejaVu Sans Mono", size: 0.9em)
#show raw.where(block: true): it => block(width: 100%, fill: cream,
  radius: 2mm, inset: (x: 3.5mm, y: 3mm), it)

#set list(body-indent: 0.6em, spacing: 0.65em)
#set enum(spacing: 0.65em)

#set table(stroke: none, inset: (x: 5pt, y: 4.2pt))
#show table.cell.where(y: 0): set text(fill: white, weight: 700)
#show table: it => {
  // Markdown tables come with auto columns. A two column table lets the
  // second take the rest, wider ones are left to fit their content.
  if it.columns.len() == 2 and it.columns.last() == auto {
    table(columns: it.columns.slice(0, -1) + (1fr,), align: it.align,
      ..it.children)
  } else {
    // A table stays on one page unless it is too tall for that.
    context {
      let inner = { set text(size: 8pt); it }
      let height = measure(block(width: 118mm, inner)).height
      block(radius: 2mm, breakable: height > 110mm,
        stroke: 0.6pt + accent.get().lighten(45%), clip: height <= 110mm,
        inner)
    }
  }
}

// Colours a chapter: headings, tables and list markers take its accent.
#let themed(colour, body) = {
  accent.update(colour)
  set table(fill: (_, y) => if y == 0 { deep(colour) }
    else if calc.even(y) { cream })
  set list(marker: diamond(fill: colour, size: 3.2pt))
  body
}

#show heading: set text(font: serif)
#show heading.where(level: 1): it => {
  pagebreak(weak: true)
  context {
    let colour = accent.get()
    let head = 21mm
    place(top + left, dx: -15mm, dy: -19mm,
      box(width: 148mm, height: head, tiled-head(head, size: 14mm)))
    v(head + band-height - band-tip - 19mm + 5mm)
    let kicker = eyebrow.get()
    if kicker != none {
      block(below: 2.2mm, text(size: 7pt, weight: 700, tracking: 0.14em,
        fill: deep(colour), upper(kicker)))
    }
    block(below: 5mm, {
      text(size: 24pt, weight: 700, fill: deep(colour), it.body)
    })
  }
}
#show heading.where(level: 2): it => context block(above: 1.35em, below: 0.7em,
  sticky: true, {
    diamond(fill: blossom, size: 4.2pt)
    h(2.2mm)
    text(size: 12.5pt, weight: 700, fill: deep(accent.get()), it.body)
  })
#show heading.where(level: 3): it => block(above: 1.3em, below: 0.6em,
  sticky: true, text(size: 10pt, weight: 700, fill: ink, it.body))
#show heading.where(level: 4): it => block(above: 1.1em, below: 0.5em,
  sticky: true, text(size: 9pt, weight: 700, fill: ink-soft, it.body))

// A boxed panel in the style of the flasher's plaques.
// It stays on one page unless it is too tall for that.
#let plaque(colour: peach, inset: 4mm, body) = context {
  let inner = block(width: 100%, radius: 2.2mm, stroke: 0.5pt + blossom,
    inset: inset, body)
  let height = measure(inner, width: 118mm - 2.4mm).height
  block(width: 100%, fill: cream-soft, radius: 3mm,
    stroke: 1.1pt + deep(colour), inset: 1.2mm, breakable: height > 140mm,
    inner)
}

// ----------------------------------------------------------------- cover

#let tiles-end = 189mm
// One quarter of a 21 mm tile holds one leaf. Everything laid over the tiles
// starts and ends on this grid, so no edge cuts through a leaf.
#let cell = 10.5mm
#let foot(body) = place(top + left, dy: tiles-end + band-line,
  block(width: 100%, height: 210mm - tiles-end - band-line,
    align(center + horizon, body)))

#page(margin: 0pt, header: none, footer: none, fill: cream-soft)[
  #tiled-head(tiles-end)
  #foot(text(font: serif, size: 10pt, style: "italic", fill: peach-deep,
    tracking: 0.04em)[wgd modular])
  #place(top + left, dx: 9 * cell, dy: cell, block(width: 4 * cell,
    height: 16 * cell, fill: cream-soft, radius: 1.5mm, clip: true,
    inset: 1.5mm, block(width: 100%, height: 100%, clip: true,
      place(top + center, image("assets/panel.png", height: 100%)))))
  #place(top + left, dx: cell, dy: 3 * cell, block(width: 7 * cell, {
    block(height: 10 * cell, fill: cream-soft, radius: 6mm,
      stroke: 2pt + peach-deep, inset: 1.6mm,
      block(width: 100%, height: 100%, radius: 4.6mm, stroke: 1pt + blossom,
        inset: (x: 3mm), align(center + horizon, {
          image("assets/mark.svg", width: 16mm)
          v(2mm)
          text(font: serif, size: 22pt, weight: 700,
            fill: peach-deep)[Peach]
          v(0.5mm)
          text(font: serif, size: 12pt, style: "italic", fill: ink-soft)[
            User manual]
          v(3.5mm)
          line(length: 30%, stroke: 0.6pt + blossom)
          v(3mm)
          set text(size: 8pt, fill: ink-soft)
          [The XIAO RA4M1 oscillator module by wgd modular and every
            firmware it runs.]
          v(4mm)
          box(fill: cream, radius: 2mm, inset: (x: 3.2mm, y: 2mm),
            stroke: 0.5pt + peach.lighten(45%),
            text(size: 7pt, weight: 700, fill: peach-deep)[
              #if data.release [Release #data.version] else [
                Development build]
              #linebreak() #month(data.date)])
        })))
  }))
]

#counter(page).update(1)

// -------------------------------------------------------------- contents

#let firmware-ids = data.firmwares.map(fw => fw.id)

#page(header: none)[
  #v(4mm)
  #text(font: serif, size: 20pt, weight: 700, fill: peach-deep)[Contents]
  #v(4mm)
  #show outline.entry.where(level: 1): it => {
    let el = it.element
    if el.has("label") and str(el.label) in firmware-ids {
      pad(left: 5mm, top: -0.2mm, text(fill: ink-soft, it))
    } else {
      v(1.6mm)
      text(weight: 700, it)
    }
  }
  #outline(title: none, depth: 1, indent: 0pt)
  #v(1fr)
  #image("assets/border.svg", width: 100%)
  #v(3mm)
  #plaque(colour: leaf, inset: 3.5mm)[
    #set text(size: 7.5pt, fill: ink-soft)
    This manual is generated from the firmware descriptions in the
    #box(link("https://github.com/" + data.repo)[peach-firmware])
    repository. It was built from commit #raw(data.commit) of
    #month(data.date).
  ]
]

// ------------------------------------------------------------ the module

#themed(peach)[
= The module

#grid(columns: (28mm, 1fr), column-gutter: 6mm,
  image("assets/panel.png", height: 110mm),
  {
    set text(size: 8.5pt)
    set par(spacing: 0.85em)
    [The Peach is a small Eurorack oscillator around the Seeed Studio XIAO
      RA4M1, a thumb-sized board with a Renesas RA4M1 microcontroller. It
      plays its waves through the microcontroller's own 12-bit DAC, and what
      it plays is defined entirely by the firmware it runs. You can flash a
      different one at any time.]
    v(1mm)
    let item(name, body) = block(below: 2.6mm, {
      text(font: serif, weight: 700, size: 9.5pt, fill: peach-deep, name)
      linebreak()
      body
    })
    item[Pitch][Coarse pitch of the oscillators.]
    item[Detune][Tunes the second oscillator against the first.]
    item[OSC 2 Level][A knob and a CV input that add up. The original
      firmware sets the level of the second oscillator with them, others
      give them a new job.]
    item[V/Oct][Pitch CV, 1 V per octave.]
    item[Wave][A button to switch the wave or the wavetable bank. The
      choice is saved and survives a power cycle.]
    item[Out][The audio output.]
  })

#pagebreak(weak: true)

== Controls in general

The firmwares read the knobs and CV inputs with the RA4M1's analog inputs.
The table shows which pin of the XIAO each control is wired to, the
firmware chapters use the same names.

#table(columns: (auto, 1fr),
  table.header([Control], [XIAO pin]),
  [Out], [A0, the DAC], [Pitch], [A1], [Detune], [A2],
  [OSC 2 Level, knob and CV], [A3], [V/Oct], [A5], [Wave], [D8],
)

Where a firmware adds a CV input to a knob, the knob sets the starting point
and the CV moves it from there. Each chapter says what its controls do.

== Specifications

#table(columns: (auto, 1fr),
  table.header([Feature], [Details]),
  [Format], [Eurorack],
  [Platform], [Seeed Studio XIAO RA4M1, Renesas RA4M1 with an Arm
    Cortex-M4 at 48 MHz],
  [Audio], [One output from the 12-bit DAC],
  [Controls], [Three knobs, two CV inputs, one button],
  [Memory], [256 KB flash, of which 240 KB for the firmware, 32 KB RAM],
  [Firmware], [Changed over USB from the browser, see the next chapter],
)
]

// -------------------------------------------------------------- flashing

#themed(leaf)[
= Flashing and building

The quickest way to change the firmware is the web flasher, which runs in
your browser and needs nothing installed.

#let flasher = "wgd-modular.github.io/peach-firmware"
+ Open #link("https://" + flasher + "/", flasher) in Google Chrome or
  Microsoft Edge. Safari and Firefox do not support WebUSB, which the
  flasher needs.
+ Connect the XIAO RA4M1 on the module to your computer with a USB-C cable.
+ Tap the *RESET* button on the XIAO twice in quick succession to enter the
  bootloader, then press *Connect* on the page and pick the device.
+ Choose a firmware and press *Flash*. It takes a few seconds, after which
  the module restarts with the new firmware.

The flasher always offers the firmwares of the latest release, together
with their controls. It can also flash a `.bin` file of your own, for
example one you built yourself. The same binaries are attached to every
#link("https://github.com/" + data.repo + "/releases/latest")[release] on
GitHub.

== Building from source

The firmwares are open source and live in the
#link("https://github.com/" + data.repo)[peach-firmware] repository on
GitHub. Everything needed to build them is managed by
#link("https://pixi.sh")[pixi], which works on Linux and macOS and on
Windows through WSL:

```sh
git clone https://github.com/wgd-modular/peach-firmware
cd peach-firmware
pixi run build peach
```

This builds a single firmware into `src/peach/build/peach.ino.bin`. Leave
out the name to build all of them, and `pixi run manual` builds this
manual. The sketches also build in the Arduino IDE, the README of the
repository describes how.
]

// ------------------------------------------------------------- firmwares

#let by-id(id) = data.firmwares.find(fw => fw.id == id)
#let colour-of(fw) = {
  let root = if fw.base == none { fw } else { by-id(fw.base) }
  accents.at(calc.rem(
    data.firmwares.filter(other => other.base == none)
      .position(other => other.id == root.id), accents.len()))
}
// A variant without sections of its own is listed in the chapter of the
// firmware it belongs to.
#let stub(fw) = fw.base != none and fw.sections.len() == 0
#let chapters = data.firmwares.filter(fw => not stub(fw))
#let variants-of(fw) = data.firmwares.filter(other => other.base == fw.id
  and stub(other))

#themed(apricot)[
= Firmwares

The Peach runs #data.firmwares.len() firmwares, each with its own
chapter that describes all of its controls. Variants of a firmware follow
it and are indented.

#let entry(fw) = {
  let colour = colour-of(fw)
  let target = if stub(fw) { by-id(fw.base) } else { fw }
  block(below: 3.4mm, breakable: false, pad(left: if fw.base == none { 0mm }
    else { 5mm }, grid(
    columns: (1fr, 8mm), column-gutter: 3mm,
    {
      link(label(fw.id), text(font: serif, weight: 700, size: 10.5pt,
        fill: deep(colour), fw.title))
      linebreak()
      text(size: 8.1pt, fill: ink-soft, fw.summary)
    },
    align(right, text(font: serif, weight: 700, size: 10.5pt,
      fill: deep(colour),
      context str(counter(page).at(label(target.id)).first()))),
  )))
}

#v(3mm)
#for fw in data.firmwares { entry(fw) }
]

#for fw in chapters {
  let colour = colour-of(fw)
  themed(colour, {
    eyebrow.update(if fw.base == none [Firmware] else [
      Variant of #by-id(fw.base).title])
    [#heading(level: 1, fw.title) #label(fw.id)]

    if fw.history != none {
      let h = fw.history
      let people = h.authors.map(a => {
        if a.login == none { a.name } else if a.login == a.name {
          link("https://github.com/" + a.login, "@" + a.login)
        } else {
          [#a.name (#link("https://github.com/" + a.login, "@" + a.login))]
        }
      })
      let key(body) = text(weight: 700, fill: deep(colour), body)
      block(below: 4mm, {
        set text(size: 7.5pt, fill: ink-soft)
        show raw: set text(size: 7.5pt)
        grid(columns: (auto, 1fr), column-gutter: 3mm, row-gutter: 1.1mm,
          key[By], people.join(", "),
          ..if fw.credit != "" { (key[Credit], fw.credit) } else { () },
          key[Added], month(h.added),
          key[Last change], [#month(h.updated) in #link("https://github.com/"
            + data.repo + "/commit/" + h.commit, raw(h.commit))],
          key[Source], link("https://github.com/" + data.repo + "/tree/main/"
            + fw.path, raw(fw.path)),
        )
      })
    }

    if fw.intro != "" { markdown(fw.intro, base: fw.path) }

    for section in fw.sections {
      heading(level: 2, section.title)
      if section.controls {
        plaque(colour: colour, markdown(section.body, base: fw.path))
        if section.rest != "" { markdown(section.rest, base: fw.path) }
      } else {
        markdown(section.body, base: fw.path)
      }
    }

    let variants = variants-of(fw)
    if variants.len() > 0 {
      heading(level: 2, "Variants")
      for variant in variants {
        [#heading(level: 3, variant.title) #label(variant.id)]
        markdown(variant.intro, base: variant.path)
      }
    }
  })
}

// ------------------------------------------------------------ back cover

#page(margin: 0pt, header: none, footer: none, fill: cream-soft)[
  #tiled-head(tiles-end)
  #foot(text(size: 8pt, fill: peach-deep,
    link("https://github.com/" + data.repo, "github.com/" + data.repo)))
  #place(top + left, dx: 3 * cell, dy: 6 * cell, block(width: 8 * cell,
    block(height: 6 * cell, fill: cream-soft, radius: 5mm,
      stroke: 2pt + peach-deep, inset: 1.6mm,
      block(width: 100%, height: 100%, radius: 3.6mm, stroke: 1pt + blossom,
        inset: (x: 7mm), align(center + horizon, {
          image("assets/mark.svg", width: 12mm)
          v(2mm)
          text(font: serif, size: 15pt, weight: 700,
            fill: peach-deep)[wgd modular]
          v(2mm)
          set text(size: 7.5pt, fill: ink-soft)
          [The original Peach firmware builds on the work of Hagiwo.
            Fraunces and Nunito Sans are licensed under the SIL Open Font
            License.]
        })))))
]
