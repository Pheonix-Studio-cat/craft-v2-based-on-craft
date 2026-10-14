# Add-ons

An add-on is a JSON file. It adds blocks, items, creatures and recipes to the
game — and nothing else.

**An add-on is data, never code.** The browser reads the fields listed below
and hands the numbers to the game; nothing in the file is executed, and there
is no field that could make it run, reach the world, read the inventory or
talk to anyone. Unknown fields are ignored. Install one in the game under
**☰ → Marketplace**, by pasting the file or a link to it.

Textures are not part of an add-on either: a block or an item gives a colour,
and the game paints the tile itself. That keeps a pack a few hundred bytes,
and it means nobody can slip a file past the browser.

## The shape of a pack

```json
{
  "name": "Example pack",
  "author": "someone",
  "blocks":    [ ... ],
  "items":     [ ... ],
  "creatures": [ ... ],
  "recipes":   [ ... ]
}
```

At most 40 blocks, 40 items, 12 creatures and 30 recipes are read per pack.

### blocks

| field | meaning |
| --- | --- |
| `name` | what it is called, up to 22 characters |
| `colour` | `"#d79a32"` or `[215, 154, 50]` |
| `speckle` | 0–30, how grainy the texture looks (default 8) |
| `seeThrough` | `true` for glass-like blocks |

### items

| field | meaning |
| --- | --- |
| `name` | up to 22 characters |
| `kind` | `material`, `tool`, `weapon`, `crossbow` or `food` |
| `colour` | as above |
| `damage` | for `weapon` and `crossbow` |
| `heal` | for `food`, in half hearts |

### creatures

| field | meaning |
| --- | --- |
| `name`, `colour` | as above |
| `health` | hit points, default 10 |
| `speed` | blocks per second, default 2.5 |
| `damage` | per hit; leave it out for a peaceful animal |
| `hostile` | `true` to make it hunt the player |
| `drops` | the name of an item (from this pack or from the game), or a number |
| `dropCount` | how many, default 1 |

Add-on creatures live in the overworld and come out at night.

### recipes

```json
{ "makes": "Amber Blade", "count": 1, "needs": [["Amber Shard", 2], ["Stick", 1]] }
```

`makes` and the names inside `needs` may be things from this pack or things
the game already has (`Stick`, `Iron Ingot`, `Cobblestone`, …). At most two
different ingredients per recipe.

## After installing

An add-on is applied when the page loads, so the game asks for a reload once
it is installed. Switching a pack off or removing it works the same way.

`web/addons/example.json` is a small complete pack; the Marketplace can
install it with one tap.
