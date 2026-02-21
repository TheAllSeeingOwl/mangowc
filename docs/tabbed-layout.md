# Tabbed Layout

## Activating the layout

```bash
# Via keybind in config
bind = SUPER, T, setlayout, tabbed

# Via mmsg command
mmsg -l TAB
```

## Moving the tabbed group to another workspace

```bash
# Move all tabbed windows to workspace 3
mmsg -s -d "tagtabbed,3"
```

## Keybind functions

| Function | Description |
|----------|-------------|
| `setlayout tabbed` | Switch current workspace to tabbed layout |
| `tagtabbed <tag>` | Move all tabbed windows to another workspace as a group (preserves tabbed layout on target). If not in tabbed layout, acts as a regular `tag`. |

## mmsg commands

| Command | Description |
|---------|-------------|
| `mmsg -l TAB` | Set current workspace layout to tabbed |
| `mmsg -s -d "tagtabbed,X"` | Move tabbed group to workspace X |

## Appearance options

| Option | Default | Description |
|--------|---------|-------------|
| `tabbar_height` | `24` | Tab bar height in pixels |
| `tabbar_font_family` | `monospace` | Font family |
| `tabbar_font_size` | `14.0` | Font size |
| `tabbar_active_bg_color` | `0.2, 0.4, 0.8, 1.0` | Active tab background (RGBA) |
| `tabbar_inactive_bg_color` | `0.15, 0.15, 0.15, 1.0` | Inactive tab background (RGBA) |
| `tabbar_active_text_color` | `1.0, 1.0, 1.0, 1.0` | Active tab text color (RGBA) |
| `tabbar_inactive_text_color` | `0.6, 0.6, 0.6, 1.0` | Inactive tab text color (RGBA) |

## Interaction

- Clicking a tab in the tab bar switches focus to that window.
