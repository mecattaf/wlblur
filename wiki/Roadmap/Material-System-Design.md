### libvisualeffects (Rust) - Phase 4 Materials

```rust
// libvisualeffects/src/materials.rs
pub enum Material {
    WindowBackground,
    Sidebar,
    Popover,
    Hud,
    Menu,
    Titlebar,
    Tooltip,
    Sheet,
    UnderWindow,
    ContentBackground,
    HeaderView,
    FullScreenUI,
}

pub struct MaterialPreset {
    // Computed from Phase 1-3 parameters
    pub blur_radius: f32,
    pub blur_passes: u32,
    pub brightness: f32,
    pub contrast: f32,
    pub saturation: f32,
    pub noise: f32,
    pub tint_color: [f32; 4],
    pub vibrancy_strength: f32,
    pub adaptive_tint: bool,
}

impl Material {
    pub fn preset(&self, appearance: Appearance) -> MaterialPreset {
        match (self, appearance) {
            (Material::Hud, Appearance::Dark) => MaterialPreset {
                blur_radius: 60.0,
                blur_passes: 4,
                tint_color: [0.063, 0.071, 0.078, 0.95],
                saturation: 1.15,
                vibrancy_strength: 1.15,
                // ...
            },
            // ... 12 materials × 2 appearances = 24 presets
        }
    }
}
```

**Quickshell integration:**
```qml
// quickshell side
BlurEffect {
    // Phase 1-3: Manual control
    blurRadius: blurSlider.value          // User tweaks
    blurPasses: passesSlider.value        // User tweaks
    tintColor: colorPicker.color          // User tweaks
    vibrancy: vibrancySlider.value        // User tweaks
    
    // Phase 4: Semantic presets (optional)
    material: VisualEffect.Material.Hud   // Or manual override
}
```

**This architecture is perfect** because:
- ✅ Quickshell users can explore parameters manually (Phases 1-3)
- ✅ libvisualeffects provides high-level abstractions (Phase 4)
- ✅ Material system becomes a "curated collection" of parameter combinations
