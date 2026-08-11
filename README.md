# OptiLogger

[![Unreal Engine](https://img.shields.io/badge/Unreal%20Engine-5.x-blue.svg)](https://www.unrealengine.com/)
[![License](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)
[![Version](https://img.shields.io/badge/Version-1.0-orange.svg)](https://github.com/yourusername/OptiLogger/releases)


## Screenshots 

</br></br>
<img width="951" height="578" alt="image" src="https://github.com/user-attachments/assets/b3054e8d-1926-48c1-afa3-bbdb4a587aae" /></br>
*The main screen, with all the information on it.*

# OptiLogger

Non-intrusive resource analysis and optimization plugin for Unreal Engine 5.x. Provides comprehensive analysis of static meshes, skeletal meshes, textures, materials, animations, audio, lighting, and post-process effects with JSON export functionality.

## Features

### Comprehensive Level Analysis
- Full level scan for all resource types with optional visibility filtering
- Detailed breakdown by asset category: meshes, textures, materials, animations, audio, lighting, and post-process volumes
- Memory usage estimation for each asset type
- LOD analysis and vertex/triangle counts for geometry

### Performance Profiling
- Shader complexity analysis with instruction counts
- Material performance metrics (vertex/pixel shader instructions)
- Texture compression and resolution tracking
- Animation keyframe and compression analysis
- Audio sample rate and compression format detection

### Visibility-Based Filtering
- Camera frustum culling for focused analysis
- Analyze only visible actors in viewport or gameplay camera
- Reduces analysis time in large levels

### Export and Reporting
- JSON export of all analysis results
- Timestamp-based file naming
- Comprehensive reports saved to project Saved directory
- Integration-ready format for external tools

### Editor Integration
- Dockable editor window with intuitive UI
- Toolbar shortcuts for quick access
- Hotkey support for all analysis functions (Ctrl + NumPad keys)
- Real-time results display with sortable columns
- Memory usage color coding (green/yellow/red thresholds)

## Requirements

- Unreal Engine 5.3 or higher
- Editor-only plugin (works in standalone editor and PIE)
- No additional dependencies beyond standard UE modules

## Installation

### Clone the Repository
```bash
git clone https://github.com/vicvasper/Optilogger_UE5.x.git
```

### Install to Project
1. Copy the `OptiLogger` folder to your project's `Plugins` directory
   ```
   YourProject/Plugins/OptiLogger/
   ```

2. Enable the plugin in Unreal Editor
   - Open your project
   - Go to **Edit > Plugins**
   - Search for "OptiLogger"
   - Check the enabled box
   - Restart the editor

3. Build the project (if using C++ project)
   - Right-click your `.uproject` file
   - Select "Generate Visual Studio Project Files"
   - Open the solution and build your project

## Usage

### Opening the Plugin

Access OptiLogger through:
- Menu: **Window > OptiLogger**
- Toolbar: OptiLogger icon in the Level Editor toolbar

### Analysis Controls

#### Toolbar Actions
- **Analyze Level**: Full scan of the current level (Ctrl+NumPad1)
- **Export Report**: Save results to JSON in `Saved/ResourceReport/` (Ctrl+NumPad2)
- **Clear**: Reset all analysis results (Ctrl+Delete)
- **Visible Only**: Toggle camera frustum filtering

#### Category-Specific Analysis
- **Static Meshes** (Ctrl+NumPad3): Vertex count, LODs, memory usage
- **Skeletal Meshes** (Ctrl+NumPad4): Bone count, animations, complexity
- **Textures** (Ctrl+NumPad5): Resolution, compression, memory usage
- **Materials** (Ctrl+NumPad6): Shader complexity, texture references
- **Animations** (Ctrl+NumPad7): Duration, compression, keyframes
- **Audio** (Ctrl+NumPad8): Compression, duration, file size
- **Lighting** (Ctrl+NumPad9): Light actors, lightmaps, performance impact
- **Post-Process** (Ctrl+NumPad0): Active effects and their configuration

#### Additional Controls
- **Ctrl+F5**: Refresh current analysis
- **Ctrl+F6**: Toggle on-screen display overlay

> These two were bound to bare **F5** and **F6** in earlier versions. Because the handler is
> installed on Slate's pre-input listener, that made them fire from anywhere in the editor —
> including while typing into a text field — so pressing F5 could start a full level analysis
> unprompted. They now require Ctrl, like every other binding.

> Opening the OptiLogger tab no longer runs an analysis automatically. The pass is synchronous
> and blocks the editor for its duration, which is a surprising cost for opening a panel. Press
> **Analyze Level** when you want one.

### Understanding Results

#### Summary Panel
Displays aggregate statistics:
- Asset counts by category
- Total estimated memory usage
- LOD distribution
- Active effects count

#### Results List
Sortable table showing:
- Asset name and type
- Memory usage (color-coded)
- Detailed metrics per asset type
- Warning indicators for high-complexity assets

#### Memory Color Coding
- Green: < 1 MB
- White: 1-10 MB
- Yellow: 10-50 MB
- Red: > 50 MB

## Technical Details

### Analysis Capabilities

**Static Meshes**
- Vertex and triangle counts per LOD
- Bounding box dimensions
- Memory estimation based on vertex data

**Skeletal Meshes**
- Bone hierarchy analysis
- Vertex count per LOD
- Animation references

**Textures**
- Resolution and aspect ratio
- Compression format detection
- Mip level count
- Virtual texture status
- Memory usage with mip chain

**Materials**
- Shader instruction counts (vertex/pixel)
- Complexity classification (Low/Medium/High)
- Texture reference count
- Blend mode detection (Opaque/Masked/Translucent)

**Animations**
- Duration and frame rate
- Keyframe count estimation
- Compression scheme detection
- Memory footprint calculation

**Audio**
- Sample rate and bit depth
- Channel configuration
- Streaming vs. loaded detection
- Uncompressed size estimation

**Lighting**
- Light type classification (Directional/Point/Spot)
- Mobility status (Static/Stationary/Movable)
- Shadow casting configuration
- Attenuation radius
- Light function detection

**Post-Process**
- Active effect enumeration
- Priority and blend settings
- Unbound volume detection
- Per-effect configuration status

### JSON Export Format

Exported reports contain:
- Export metadata (date, engine version, plugin version)
- Categorized asset arrays
- Full metrics for each analyzed asset
- Hierarchical structure for easy parsing

Example structure:
```json
{
  "ExportDate": "2025-01-24 12:00:00",
  "PluginVersion": "1.0",
  "EngineVersion": "5.3.0",
  "StaticMeshes": [...],
  "SkeletalMeshes": [...],
  "Textures": [...],
  ...
}
```

## Performance Considerations

- Analysis runs synchronously on the game thread and blocks the editor while it completes
- Large levels may take several seconds to analyze
- Visibility filtering significantly reduces analysis time in complex scenes
- Results are cached until cleared or refreshed
- JSON export is lightweight and fast

## Known Limitations

- Editor-only functionality (not available in packaged builds)
- Some metrics require WITH_EDITOR compilation flag
- Shader instruction counts only available in editor builds
- Animation analysis relies on loaded assets in memory

## Troubleshooting

### No Analysis Results
- Ensure you have a valid level open
- Check that actors exist in the scene
- Verify plugin is enabled in Edit > Plugins
- Try disabling "Visible Only" filter

### Export Fails
- Confirm write permissions to project Saved directory
- Check disk space availability
- Verify JSON serialization hasn't been modified

### Missing Metrics
- Some metrics require editor build configuration
- Ensure assets are fully loaded before analysis
- Check that asset references are valid

## Development

### Module Structure
- **OptiLogger**: Main module with editor subsystem and UI
- **ResourceAnalyzer**: Core analysis logic
- **OptiloggerWidget**: Slate-based UI implementation
- **OptiloggerSubsystem**: Editor subsystem for input handling

### Key Classes
- `UResourceAnalyzer`: Asset analysis engine
- `UOptiloggerSubsystem`: Editor subsystem and input router
- `SOptiloggerWidget`: Main UI widget
- `FOptiloggerCommands`: Command registration and hotkeys

### Extending Functionality

To add new analysis categories:
1. Define analysis data struct in `ResourceAnalyzer.h`
2. Implement analysis function in `ResourceAnalyzer.cpp`
3. Add UI button in `OptiloggerWidget.cpp`
4. Register hotkey in `OptiloggerCommands.cpp`
5. Update JSON export in `ResourceAnalyzer::ExportAnalysisToJSON()`

## Version History

### 1.0 (Current)
- Initial release for Unreal Engine 5.3
- Comprehensive asset analysis
- Visibility-based filtering
- JSON export functionality
- Hotkey support
- Editor UI integration

## License

MIT License - See LICENSE file for details

## Author

Created by Victor Rivas ([@vicvasper](https://github.com/vicvasper))

For bug reports, feature requests, or contributions, please open an issue on GitHub.

## Links

- Repository: [github.com/vicvasper/Optilogger_UE5.3](https://github.com/vicvasper/Optilogger_UE5.3)
- Portfolio: [vicvasper.github.io/README](https://vicvasper.github.io/README/)
- LinkedIn: [linkedin.com/in/victorrivasperez](https://www.linkedin.com/in/victorrivasperez/)


