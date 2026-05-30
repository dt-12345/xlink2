# XLINK Tool

A tool for editing XLink2 database files used by many Nintendo EPD games. XLink2 is a library used for managing the emission of VFX and sounds, combining the previously separate ELink and SLink libraries.

Note that this is still a WIP and there may be bugs. Feel free to report any you any come across.

Huge thanks to [Shadow](https://github.com/shadowninja108/WoomLink) whose research served as a great starting point and reference.

## Basic Usage

```
xlink -i <input_file_path> -o <output_file_path>
```

Running
```
xlink --help
```
will print a help message with more detailed usage information.

## Documentation (WIP)

Sample File
```
Metadata {
  ModuleType = SLink
}
ParamDefines {
  SystemUserParams {
    GroupName = ""
    DistanceParamSetName = ""
    LimitType = 0x0
    PlayableLimitNum = -1
    Priority = 0.5
    DopplerFactor = -1.0
    ArrangeGroupParams = <custom>
    BitFlag = 0
  }
  CustomUserParams {
  }
  SystemAssetParams {
    AssetName = ""
    RuntimeAssetName = ""
    GroupName = ""
    Volume = 1.0
    Pitch = 1.0
    Lpf = 0.0
    StartTimePosType = 0x0
    StartTimePos = 0.0
    StopFrame = 0.0
    FadeInTime = 0.0
    FadeType = 0x0
    Delay = 0.0
    Duration = 0.0
    Priority = 0.5
    DopplerFactor = -1.0
    StereoWidth = 1.0
    Bone = ""
    DistanceParamSetName = ""
    BitFlag = 0
  }
  CustomAssetParams {
  }
  TriggerParams {
    Volume = 1.0
    Pitch = 1.0
    Lpf = 0.0
    StartTimePos = 0.0
    StopFrame = 0.0
    FadeInTime = 0.0
    Delay = 0.0
    Priority = 0.5
    Bone = ""
  }
}
Users {
  MyUser {
    UserParams {
      GroupName = ""
      DistanceParamSetName = ""
      LimitType = 0x0
      PlayableLimitNum = -1
      Priority = 0.5
      DopplerFactor = -1.0
      ArrangeGroupParams = ARRANGE {
      }
      BitFlag = 0b0
      ManualDuckingName = ""
      ShapeListFileName = ""
    }
    LocalProperties {
    }
    ActionSlots {
      Slot0 {
        Action0 {
          0x12345678 {
            Type = Always
            Start = 0
            End = 0
            Unknown1 = 0
            Unknown2 = 2079
            Oneshot = true
            Asset = MyAsset[0x98765432]
          }
        }
      }
    }
    AssetCallTables {
      MyAsset[0x98765432] {
        EmitCount = 1
        Oneshot = false
        NoPause = false
        Execute = Asset {
          AssetName = "MyAsset"
          RuntimeAssetName = "MyAsset"
          GroupName = ""
          BitFlag = 0b1001
        }
      }
    }
  }
}
```

### Metadata

This is metadata that can be ignored.

### ParamDefines

These are definitions for all parameters used in the file (User, Asset, and Trigger) and specify the default values for each. It is advised to not modify this section unless you are absolutely sure of what you are doing (and if you have to ask, you don't).

### Users

XLink2 functions through a system of **users**. Each user has a set of **asset call tables** which they can use.

#### UserParams

User-specific parameters.

#### LocalProperties

Assigned local properties for the user.

#### ActionSlots

**Action slots** represent assignable slots that can be filled with an **action** (an action is some external action to the XLink2 system such as AS). For a given action in a given slot, an action can trigger a call table through an **action trigger**.

#### Properties

The value of a property can trigger a call table through a **property trigger** similar to an action trigger.

#### AlwaysTriggers

**Always triggers** represent call tables that are always emitted for a given user even without explicit request.

#### AssetCallTables

Each asset call table can either by an **asset** or a **container**. Assets directly correspond to an asset (VFX or sound) while containers allow for stringing together other call tables. Each call table is addressed by its key and also must have a unique GUID (the specific value doesn't matter). When an application wishes to emit an effect/sound, it will do so by searching for an asset call table with the matching key for the specified user.

##### Assets

Each asset corresponds to a VFX or a sound and has a set of **asset params**. These params are what link the asset to the corresponding resource file and determine how the asset will be played.

##### Containers

There are 8 types of containers with different functionality.

- Switch
  - Selects one child call table based on some condition (action slot or property)
- Random
  - Randomly selects one child call table
- RandomNoRepeat
  - Randomly selects one child call table without repeating
- Blend
  - Blends together all child call tables
- BlendBy
  - Blends together two child call tables based on some condition (property)
- Sequence
  - Plays each child call table in sequence
- Grid (Stardust and above)
  - Selects one child call table based on the value of two properties
- Jump (EXKing and above)
  - Jumps to another call table outside of the current container

### Other Stuff I Need to Organize

AssetBitFlag: (I haven't looked at these in a while so I'm not sure how accurate these are - they may also change by version, also why don't I have bit 0 smh)
  - ELink:
    - bit 1 = IsFollow
    - bit 2 = IsUseOneEmitter
    - bit 3 = IsForceLoopAsset
  - SLink:
    - bit 1 = IsNoParamUpdate
    - bit 2 = IsNoPos
    - bit 3 = IsStopWhenEmitterDestroying
    - bit 4 = IsUnified
    - bit 5 = IsAutoOneTimeFade
    - bit 6 = IsForceLoopAsset

## Building

Requires CMake 3.18+ and gcc 16+
```
cmake -B build -DCMAKE_BUILD_TYPE=Release # -G "Ninja" or whatever if you want
cmake --build build
```