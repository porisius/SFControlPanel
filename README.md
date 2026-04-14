# Satisfactory Control Panel (SCP)

[![Official Discord Server](https://img.shields.io/badge/Official%20Discord%20Server-232634?style=for-the-badge&logo=discord&logoColor=232634&color=F2C800)](https://discord.gg/c6446HTHpu)

Ever tire of scrolling through menus and hotbars to find that one blueprint, building, or foundation in the game to get to?

Welcome to SCP where you can use your Elgato Stream Deck or your own smartphone/tablet with the Elgato Mobile App. You can also use anything you wish to connect to the mod to use its features. This is not restricted to the Elgato Ecosystem, however, I have no plans to actively support anything else. However, I will provide the API below so that you can create your own.

This was inspired from another mod/plugin created by Kaz Wolfe called [XIVDeck](https://github.com/KazWolfe/XIVDeck). So, special thanks for his permission to use his code to help develop this mod.

## Elgato Stream Deck Mobile Apps

Download:  
[Android](https://play.google.com/store/apps/details?id=com.corsair.android.streamdeck&hl=en_US)  
[iOS](https://apps.apple.com/us/app/elgato-stream-deck-mobile/id1440014184)

Information:  
[Elgato Official Mobile App Site](https://www.elgato.com/us/en/s/stream-deck-mobile)


## Example

![Teaser Image](https://github.com/porisius/SFControlPanel/blob/main/Resources/example.jpg "Teaser Image")

## Getting Started

### Multiplayer

This mod has been tested on a dedicated server. There are no server builds because this is a Client only mod. While everyone does have to have the Satisfactory Mod Loader installed for multiplayer, not everyone is required for this mod. If you planning to use with a Dedicated Server, the server does require the Mod Loader to be compatible.

### Installing the Plugin

The Control Panel plugins are available for download from this repository's [Releases page](https://github.com/porisius/SFControlPanel/releases/latest).  
To install the Stream Deck plugin, simply open the SFControl.streamDeckPlugin file. The Elgato Stream Deck software will take care of all installation steps.  
To install the Satisfactory Control Panel Mod, simply add it through the [Satisfactory Mod Manager](https://smm.ficsit.app).

## How this mod works

The mod opens a HTTP Web Server port on TCP 59384 (configurable in later releases) on the localhost adapter (127.0.0.1). This listens for a Stream Desk or any device that sends HTTP GET Requests to the Mod loaded to Satisfactory.

Simple `curl` commands are able to be used to test this mod.

## API List

### /api/v1/action/list

Returns list of blueprints for the save/server in JSON.
This is what is needed to pass along to `/api/v1/:descriptor/execute`

```json
[
  {
    "friendlyName":"Build Gun",
    "descriptor":"build"
  },
  {
    "friendlyName":"Dismantle",
    "descriptor":"dismantle"
  },
  {
    "friendlyName":"Customizer",
    "descriptor":"paint"
  },
  {
    "friendlyName":"Holster",
    "descriptor":"holster"
  },
  {
    "friendlyName":"Inventory",
    "descriptor":"inventory"
  },
  {
    "friendlyName":"Photo Mode",
    "descriptor":"camera"
  },
    {
    "friendlyName":"Third-Person",
    "descriptor":"thirdperson"
  },
]
```

### /api/v1/blueprints/list

Returns list of blueprints for the save/server in JSON.
This is what is needed to pass along to `/api/v1/:descriptor/blueprint`

```json
[
  {
    "friendlyName": "Copper Refinery - L"
    "descriptor": "Copper Refinery - L"
  }
]
```

### /api/v1/customizer/list

Returns list of build gun recipes for the save/server in JSON.
This is what is needed to pass along to `/api/v1/:descriptor/build`

```json
[
  {
    "friendlyName": "HUB Terminal",
    "descriptor": "Desc_TradingPost_C"
  }
]
```

### /api/v1/recipes/list

Returns list of build gun recipes (buildables and vehicles) for the save/server in JSON.
This is what is needed to pass along to `/api/v1/:descriptor/build`

```json
[
  {
    "friendlyName": "HUB Terminal",
    "descriptor": "Desc_TradingPost_C"
  }
]
```

### /api/v1/swatch/list

Returns list of swatches for the save/server in JSON.
This is what is needed to pass along to `/api/v1/:descriptor/build`

```json
[
  {
    "friendlyName":"Concrete Structure Swatch",
    "descriptor":"SwatchDesc_Concrete_C",
    "PrimaryColor":"FFFFFFFF",
    "SecondaryColor":"FA9549FF"
  }
]
```

### /api/v1/:action/execute

Toggles various actions, see below:

* `build` - Toggles build gun
* `dismantle` - Toggles dismantle mode
* `paint` - Toggles customizer
* `holster` - Toggles holstering your current equipped item
* `inventory` - Toggles inventory window
* `thirdperson` - Toggles 3rd Person Mode
* `camera` - Toggles Photo Mode

### /api/v1/:descriptor/build

Activates the build gun and selects a valid build gun recipe (Buildable or Vehicle). Use list from `/api/v1/recipe/list`<br>
If successful, returns `200 OK`.

### /api/v1/:descriptor/blueprint

Activates the build gun and selects the desired blueprint. Use list from `/api/v1/blueprint/list`<br>
Spaces, or `%20` in the URL, are handled by the UE uPlugin.<br>
If successful, returns `200 OK`.

### /api/v1/:descriptor/customizer

Activates the build gun and selects the desired Customizer Pattern. Use list from `/api/v1/customizer/list`<br>
If successful, returns `200 OK`.

### /api/v1/:descriptor/swatch

Activates the build gun and selects the desired Swatch. Use list from `/api/v1/swatch/list`<br>
If successful, returns `200 OK`.

### /api/v1/:descriptor/icon

Returns png icon file of descriptor (blueprint, customizer, or build). Use list from respective type's list API.<br>
If successful, returns `200 OK`.

## Not yet implemented

### /api/v1/:descriptor/scan

Activates the resource scanner and pings for selected resource. Use list from `not yet implemented`<br>
If successful, returns `200 OK`.<br>
Note: This is due to being unable to figure out how to use the Resource Scanner. If you would like to assist, please see the `SFControlPanelSubsystem.h/.cpp` uPlugin.

## Known Bugs

[![Control Panel GitHub Issues](https://img.shields.io/github/issues/porisius/SFControlPanel?logoColor=232634&color=F2C800)](https://github.com/porisius/FicsitRemoteMonitoring/issues)

## Feedback

If you have suggestions or feature requests, don't hesitate to drop by the [Discord Server](https://discord.gg/c6446HTHpu).

## Permissions Granted

Feel free to create your own interfaces to interact with this mod

## Contribution

Please make sure to read the [Contributing Guide](https://github.com/porisius/SFControlPanel/blob/dev/CONTRIBUTING.md).
