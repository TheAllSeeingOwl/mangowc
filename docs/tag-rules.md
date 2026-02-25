# Tag Rules

Tag rules let you configure per-tag defaults (layout, gaps, master count, etc.) and optionally rename workspaces. Rules are matched per-monitor, so different monitors can have different tag configurations.

## Basic syntax

```
tagrule = id:<tag>, <key>:<value>, <key>:<value>, ...
```

- `id` is required and identifies which tag (1–20) the rule applies to. `id:0` refers to the overview workspace.
- Additional fields are optional and can be combined freely.

## Available fields

| Field | Type | Description |
|-------|------|-------------|
| `id` | integer (0–20) | Tag index to apply this rule to |
| `name` | string | Custom display name for the workspace |
| `layout_name` | string | Default layout for this tag (e.g. `monocle`, `tile`, `tabbed`) |
| `mfact` | float (0.1–0.9) | Master area size ratio |
| `nmaster` | integer (1–99) | Number of windows in master area |
| `no_render_border` | 0 or 1 | Hide window borders on this tag |
| `no_hide` | 0 or 1 | Always show this workspace in status bars even when empty |
| `monitor_name` | regex | Match only monitors whose output name matches |
| `monitor_make` | string | Match only monitors by make |
| `monitor_model` | string | Match only monitors by model |
| `monitor_serial` | string | Match only monitors by serial |

## Examples

### Rename workspaces

```
tagrule = id:1, name:terminal
tagrule = id:2, name:browser
tagrule = id:3, name:code
tagrule = id:10, name:music
tagrule = id:11, name:chat
```

### Set a default layout

```
tagrule = id:2, layout_name:monocle
tagrule = id:5, layout_name:tabbed
```

### Combine name and layout

```
tagrule = id:2, name:browser, layout_name:monocle
tagrule = id:5, name:media, layout_name:tabbed, no_render_border:1
```

### Per-monitor rules

Rules without a monitor filter apply to all monitors. Add a monitor filter to scope a rule to a specific display:

```
tagrule = id:1, monitor_name:eDP-1, name:local
tagrule = id:1, monitor_name:HDMI-A-1, name:remote
```

`monitor_name` supports regex, so `HDMI.*` would match any HDMI output.

### Keep empty workspaces visible in the bar

```
tagrule = id:10, name:music, no_hide:1
```

## Tags 1–20

Mango supports up to 20 tags. The default names are `"1"` through `"20"`. Use `tagrule` with `name:` to override them.

To bind keys to tags 10–20, use the tag number as the argument to `view`, `toggleview`, `tag`, or `toggletag`:

```
bind = SUPER, F1, view, 10
bind = SUPER SHIFT, F1, tag, 10
bind = SUPER, F2, view, 11
bind = SUPER SHIFT, F2, tag, 11
```

Or use the `|` syntax to target multiple tags at once:

```
bind = SUPER, F1, view, 10|11
```

## Notes

- Rules are applied when a monitor is created and re-applied on config reload.
- Workspace names set via `name:` are reflected in the ext-workspace protocol, so status bars and workspace switchers that support it will show the custom names.
- If multiple rules match the same tag and monitor, they are applied in config file order — later rules override earlier ones.
