# XLINK Tool

A tool for editing XLink2 database files used by many Nintendo EPD games. XLink2 is a library used for managing the emission of VFX and sounds, combining the previously separate ELink and SLink libraries. Currently supports all Switch-era games, support for other versions may be added at a later date.

Note that this is still a WIP and there may be bugs. Feel free to report any you any come across.

Huge thanks to [Shadow](https://github.com/shadowninja108/WoomLink) whose research served as a great starting point and reference.

`{fmt}` is licensed under a permissive MIT license, the rest of the code is licensed under GPLv3.

## Basic Usage

```sh
xlink -i <input_file_path> -o <output_file_path>
```

Note: for games other than *Tears of the Kingdom*, please specify the game with the `--game` argument.

Running
```sh
xlink --help
```
will print a help message with more detailed usage information.

## Documentation (WIP)

Note that identifiers containing spaces or any of the following characters must be quoted: `"`, `=`, `!`, `<`, `>`, `(`, `)`, `{`, `}`, `[`, `]`, `@`, `,`, `#`, `:`.

### Metadata

This is metadata for the file. `Metadata` defines whether this is an ELink or a SLink database.

Example:
```
Metadata {
  ModuleType = SLink
}
```

### ParamDefines

These are definitions for all parameters used in the file (User, Asset, and Trigger) and specify the default values for each. It is advised to not modify this section unless you are absolutely sure of what you are doing (and if you have to ask, you don't).

User params are for individual users, asset params are for individual assets, and trigger params are for triggers (action, property, or always) and replace values in the triggered asset's asset param.

Example:
```
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
```

#### Parameter and Value Types

- S32
  - 32-bit signed integer
  - Treated as a bitfield if in binary
- F32
  - 32-bit floating point number
  - Curve
    - Interpolate an input property along a pre-defined curve
    - Curves can be of type Standard or Constant
      - Standard represents a curve that interpolates the input value between a set of pre-defined points
      - Constant represents a curve that always returns the same value (one point)
    - Curves can have one of two update types
      - Update is for curves that continuously update the property value during emission
      - NoUpdate is for curves that do not update the property value during emission (only use the value at the time of emit)
  - Random
    - Chooses a random value between the specified min and max values using the specified distribution (the polynomial types can powers 1.5, 2, 3, or 4)
      - Linear
      - InflectedPolynomial
        - Concave up from min to midpoint then convex up from midpoint to max
      - IncreasingPolynomial
      - DecreasingPolynomial
- Bool
  - Boolean value (true or false)
- Enum
  - Enum value (in hex)
- String
  - String value (enclosed in quotes)
- ArrangeParam
  - SLink exclusive, controls how sounds are limited
  - List of ArrangeGroups
    - `LimitType` controls which sounds in the group are the highest priority (silenced last)
      - Types: None, PriorityThenOldest, PriorityThenNewest, OldestThenPriority, NewestThenPriority, SpatialPriorityThenOldest, SpatialPriorityThenNewest, OldestThenSpatialPriority, NewestThenSpatialPriority
    - `Threshold` controls how many sounds must be present before the limiter takes effect
    - `IncludeFading` controls whether or not the group includes fading sounds in its count (TODO: this might be exclusive to newer versions)

### Users

XLink2 functions through a system of **users**. Each user has a set of **asset call tables** which they can use. The file does not directly assign users names, instead, a CRC-32 hash of the name is stored. If a known username can be determined for the user, it will appear in the text output, otherwise, the CRC-32 hash will appear instead.

#### UserParams

User-specific parameters, what params are available is set by the ParamDefines.

Example:
```
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
```

#### LocalProperties

Assigned local properties for the user.

Example:
```
LocalProperties {
  サウンドマテリアル
  水マテリアル
  水平速度
  "深さ(Rea)"
  足速度
}
```

#### ActionSlots

**Action slots** represent assignable slots that can be filled with an **action** (an action is some external action to the XLink2 system such as AS). For a given action in a given slot, an action can trigger a call table through an **action trigger**.

Example:
```
ActionSlots {
  "AS[0]" {
    Lv1 {
      0x1875abb1 {
        Type = FrameWindow
        Start = 1
        End = 2147483647
        Unknown1 = 0
        Unknown2 = 2079
        Oneshot = false
        Asset = Crack[0x7cf32aba]
      }
    }
    Lv2 {
      0x88abab21 {
        Type = FrameWindow
        Start = 2
        End = 2147483647
        Unknown1 = 0
        Unknown2 = 2079
        Oneshot = false
        Asset = Crack[0x7cf32aba]
      }
    }
  }
}
```
`AS[0]` is the name of the action slot, with `Lv1` and `Lv2` being actions that can be assigned to the slot. Under each action is a list of action triggers that are triggered by the slot being filled with that action. For example, `0x1875abb1` is the GUID of the action trigger triggered by `AS[0]` being set to `Lv1`. The value of the GUID itself is not important, however, it should be unique for a given user's action triggers. There are four types of action triggers which differ in their trigger. `FrameWindow` action triggers are triggered when the action slot is filled with the correct action and the action's current progress falls within the specified frame window. `Always` action triggers are triggered unconditionally when the correct action fills the action slot. `OnLeave` action triggers are triggered when the correct action "leaves" the action slot. Finally, `Previous` action triggers are triggered when the correct action fills the action slot and the action previously in that slot matches the specified condition. `Oneshot` means that the trigger can only fire a single time when the action fills the slot. In order to re-trigger, a oneshot action trigger must wait until the action leaves the slot to reset.

#### Properties

The value of a property can trigger a call table through a **property trigger** similar to an action trigger.

Example:
```
Local::回転速度 {
  if <value> > 0.00100000005 => 0xff7af474 {
    Lazy = false
    Unknown = 2079
    Asset = Roll[0xdd1b70ea]
  }
}
```
`Local::回転速度` is the property in question, with `Local` specifying that it is a local property as opposed to a global property. `if <value> > 0.00100000005` specifies the condition that needs to be met to trigger the corresponding property trigger. `<value>` is a special symbol here that refers to the value of the property. `0xff7af474` is the GUID of the property trigger (see action triggers for more on trigger GUIDs). If a property trigger is `Lazy`, it will not trigger if the property starts out fulfilling the condition; instead, the property must change value before it can trigger.

#### AlwaysTriggers

**Always triggers** represent call tables that are always emitted for a given user even without explicit request.

Example:
```
0x81084c1e {
  Flags = 0
  Unknown = 2079
  Asset = Bullet_FlyStart[0x0c6e9a02]
}
```
`0x81084c1e` is the GUID of the always trigger (see action triggers for more on trigger GUIDs).

#### AssetCallTables

Each asset call table can either by an **asset** or a **container**. Assets directly correspond to an asset (VFX or sound) while containers allow for stringing together other call tables. Each call table is addressed by its key and also must have a unique GUID (the specific value doesn't matter, it just needs to be unique among a given user's asset call tables). When an application wishes to emit an effect/sound, it will do so by searching for an asset call table with the matching key for the specified user. All asset call tables in the file are referenced by the syntax `Key[GUID]`. All top-level asset call tables should have a unique key as the key is how the game references and accesses a given call table. `<null>` is a special symbol referring to a null call table and is only permitted in grid containers and jump containers.

Example:
```
Death_Burned[0x7b38f283] {
  EmitCount = 1
  Oneshot = false
  NoPause = false
  UserFlags = 0b0
  Execute = Asset {
    AssetName = "Death_Burned"
    RuntimeAssetName = "SE_BurnedOut"
    GroupName = "Chemical"
    Priority = 0.550000012
    BitFlag = 0b1010
  }
}
```
`Death_Burned` is the key and `0x7b38f283` is the GUID. `EmitCount` controls the number of times the corresponding asset is played when this call table is triggered. `Oneshot` controls whether a given asset call table must be continuously requested in order to continue emitting (TODO: double check this). `NoPause` controls whether or not this specific asset call table is pauseable. `UserFlags` are an 8-bit, game-specific set of flags. For example, in *Tears of the Kingdom*, the second lowest bit of user flags is used to control whether the `_Miasma` variant of a sound should be played if applicable. `Execute` controls what this call table actually does (this example is an asset), see below for more details. A call table may require observation (`IsNeedObserve`) if any of it or any of its children fulfill at least one of the following: an emit count of -1 or a looping asset. If observation is required, the behavior of certain containers will change. `Switch`, `BlendBy`, and `Grid` containers will dynamically update the active child container(s) rather than selecting only when starting. `Sequence` containers will not automatically continue to the next container (see below for more details).

##### Assets

Each asset corresponds to a VFX or a sound and has a set of **asset params**. These params are what link the asset to the corresponding resource file and determine how the asset will be played.

##### Containers

There are 8 types of containers with different functionality. Containers may only call their own child call tables with the exception of jump containers.

- Switch
  - Selects one child call table based on some condition (action slot or property)
    - Note that action slot switch containers are only available on Stardust and above
  - `_` denotes the default case
```
@Unknown = -1
Execute = Switch (ActionSlot::Act) {
  (<value> == <action>::InterruptDie) => VoiceRagdollLandDead[0x8d1ca611] {
    EmitCount = 1
    Oneshot = false
    NoPause = false
    UserFlags = 0b0
    Execute = Asset {
      AssetName = "VoiceRagdollLandDead"
      RuntimeAssetName = "@Blank"
      GroupName = "ActorVoice"
      Volume = 0.5
      StopFrame = 2.0
      BitFlag = 0b1001
    }
  }
  (_) => RagdollLandVoice_00[0xcb2bcc92] {
    EmitCount = 1
    Oneshot = false
    NoPause = false
    UserFlags = 0b0
    Execute = Asset {
      AssetName = "RagdollLandVoice_00"
      RuntimeAssetName = "@Blank"
      GroupName = "ActorVoice"
      Volume = 0.5
      StopFrame = 2.0
      BitFlag = 0b1001
    }
  }
}
```
- Random
  - Randomly selects one child call table (each child has a specific weight controlling its probability)
```
Execute = Random {
  1.0 => vElectric_00[0xde1822f6] {
    EmitCount = 1
    Oneshot = false
    NoPause = false
    UserFlags = 0b10
    Execute = Asset {
      AssetName = "vElectric_00"
      RuntimeAssetName = "Octarock_Vo_Damage_Electric00"
      GroupName = "ActorVoice"
      StopFrame = 2.0
      Priority = 0.589999974
      Bone = "Head_1"
      BitFlag = 0b1001
    }
  }
  1.0 => vElectric_01[0x99de90c2] {
    EmitCount = 1
    Oneshot = false
    NoPause = false
    UserFlags = 0b10
    Execute = Asset {
      AssetName = "vElectric_01"
      RuntimeAssetName = "Octarock_Vo_Damage_Electric01"
      GroupName = "ActorVoice"
      StopFrame = 2.0
      Priority = 0.589999974
      Bone = "Head_1"
      BitFlag = 0b1001
    }
  }
}
```
- RandomNoRepeat
  - Randomly selects one child call table without repeating (each child has a specific weight controlling its probability)
```
Execute = RandomNoRepeat {
  1.0 => Greeting_00[0x42635d7f] {
    EmitCount = 1
    Oneshot = false
    NoPause = false
    UserFlags = 0b0
    Execute = Asset {
      AssetName = "Greeting_00"
      RuntimeAssetName = "DummyBlank"
      GroupName = "NpcVoice"
      Bone = "Head"
      BitFlag = 0b1001
      ArrangeAssetName = "NV_Greeting00"
    }
  }
  1.0 => Greeting_01[0x8d2d52d8] {
    EmitCount = 1
    Oneshot = false
    NoPause = false
    UserFlags = 0b0
    Execute = Asset {
      AssetName = "Greeting_01"
      RuntimeAssetName = "DummyBlank"
      GroupName = "NpcVoice"
      Bone = "Head"
      BitFlag = 0b1001
      ArrangeAssetName = "NV_Greeting01"
    }
  }
}
```
- Blend
  - Blends together all child call tables
```
Execute = Blend {
  Frozen_Start[0xf32ea6ba] {
    EmitCount = 1
    Oneshot = false
    NoPause = false
    UserFlags = 0b0
    Execute = Asset {
      AssetName = "Frozen_Start"
      RuntimeAssetName = "Frozen_Start"
      GroupName = "Chemical"
      Priority = 0.550000012
      BitFlag = 0b1001
    }
  }
  Frozen_[0x749dcc1d] {
    EmitCount = 1
    Oneshot = false
    NoPause = false
    UserFlags = 0b0
    Execute = Asset {
      AssetName = "Frozen_"
      RuntimeAssetName = "Frozen"
      GroupName = "Chemical"
      Priority = 0.349999994
      BitFlag = 0b11001
    }
  }
}
```
- BlendBy (Stardust and above)
  - Blends together two child call tables based on a property condition
```
@Unknown = -1
Execute = BlendBy (Local::"深さ(Rea)") {
  ([Min: 0.100000001, Op: SquareRoot], [Max: 10.0, Op: Multiply]) => WaterRapid_00[0x2142ed2d] {
    EmitCount = 1
    Oneshot = false
    NoPause = false
    UserFlags = 0b0
    Execute = Asset {
      AssetName = "WaterRapid_00"
      RuntimeAssetName = "FldObj_Zora_AncientFacilitySpout_WaterRapid"
      GroupName = "Actor"
      StopFrame = 20.0
      FadeInTime = 0.5
      BitFlag = 0b1001
    }
  }
  ([Min: 0.0, Op: Multiply], [Max: 3.5, Op: SquareRoot]) => WaterRapid_InWater[0xf673d63f] {
    EmitCount = 1
    Oneshot = false
    NoPause = false
    UserFlags = 0b0
    Execute = Asset {
      AssetName = "WaterRapid_InWater"
      RuntimeAssetName = "AncientFacilitySpout_WaterRapid_InWater"
      GroupName = "Actor"
      StopFrame = 60.0
      FadeInTime = 0.5
      BitFlag = 0b1001
    }
  }
}
```
- Sequence
  - Plays each child call table in sequence
    - `ForceContinue` is only used if the container needs observing (`IsNeedObserve = true`)
      - If 0, then when this child completes, the sequence container is considered to have finished a single emission (of the total `EmitCount`)
      - Otherwise, when this child completes, the sequence container will continue on to the next child instead
```
Execute = Sequence {
  @ForceContinue = 0
  Blank[0x29e1f833] {
    EmitCount = 1
    Oneshot = false
    NoPause = false
    UserFlags = 0b0
    Execute = Asset {
      AssetName = "Blank"
      RuntimeAssetName = "@Blank"
      GroupName = "Actor"
      StopFrame = 60.0
      Duration = 150.0
      Priority = 0.449999988
      BitFlag = 0b1001
    }
  }
  @ForceContinue = 1
  onCalcSkipStart_Act_Carpenter_Carpenter_00[0x171321ed] {
    EmitCount = 1
    Oneshot = false
    NoPause = false
    UserFlags = 0b0
    Execute = Asset {
      AssetName = "onCalcSkipStart_Act_Carpenter_Carpenter_00"
      RuntimeAssetName = "Npc_Hylia_M_Hit_Wood"
      GroupName = "Actor"
      StopFrame = 60.0
      Priority = 0.449999988
      BitFlag = 0b1001
      ReturnTimeFromCalcSkip = 0.699999988
    }
  }
}
```
- Grid (Stardust and above)
  - Selects one child call table based on the value of two properties
  - `Cases` specifies the combinations of values and the corresponding call table
  - `Children` defines the container's child call tables (all cases must either refer to one of these call tables or be null)
```
Execute = Grid (Local::武器振り方向に対するサイズ, Local::攻撃種類) {
  Cases {
    (武器振り方向に対するサイズ::L, 攻撃種類::コンボフィニッシュ) => "(コンボフィニッシュ, S)"[0x1a4a2df5]
    (武器振り方向に対するサイズ::L, 攻撃種類::ジャンプ攻撃) => "(ジャンプ攻撃, S)"[0x2a5c3e5c]
    (武器振り方向に対するサイズ::L, 攻撃種類::通常攻撃) => "(通常攻撃, S)"[0xf3c39b9e]
    (武器振り方向に対するサイズ::L, 攻撃種類::特殊攻撃) => "(通常攻撃, S)"[0xf3c39b9e]
    (武器振り方向に対するサイズ::L, 攻撃種類::溜め攻撃) => "(溜め攻撃, S)"[0x872a995d]
    (武器振り方向に対するサイズ::L, 攻撃種類::溜め中攻撃) => "(溜め中攻撃, SandM)"[0xdd06dbe3]
    (武器振り方向に対するサイズ::M, 攻撃種類::コンボフィニッシュ) => "(コンボフィニッシュ, S)"[0x1a4a2df5]
    (武器振り方向に対するサイズ::M, 攻撃種類::ジャンプ攻撃) => "(ジャンプ攻撃, S)"[0x2a5c3e5c]
    (武器振り方向に対するサイズ::M, 攻撃種類::通常攻撃) => "(通常攻撃, S)"[0xf3c39b9e]
    (武器振り方向に対するサイズ::M, 攻撃種類::特殊攻撃) => "(通常攻撃, S)"[0xf3c39b9e]
    (武器振り方向に対するサイズ::M, 攻撃種類::溜め攻撃) => "(溜め攻撃, S)"[0x872a995d]
    (武器振り方向に対するサイズ::M, 攻撃種類::溜め中攻撃) => "(溜め中攻撃, SandM)"[0xdd06dbe3]
    (武器振り方向に対するサイズ::S, 攻撃種類::コンボフィニッシュ) => "(コンボフィニッシュ, S)"[0x1a4a2df5]
    (武器振り方向に対するサイズ::S, 攻撃種類::ジャンプ攻撃) => "(ジャンプ攻撃, S)"[0x2a5c3e5c]
    (武器振り方向に対するサイズ::S, 攻撃種類::通常攻撃) => "(通常攻撃, S)"[0xf3c39b9e]
    (武器振り方向に対するサイズ::S, 攻撃種類::特殊攻撃) => "(通常攻撃, S)"[0xf3c39b9e]
    (武器振り方向に対するサイズ::S, 攻撃種類::溜め攻撃) => "(溜め攻撃, S)"[0x872a995d]
    (武器振り方向に対するサイズ::S, 攻撃種類::溜め中攻撃) => "(溜め中攻撃, SandM)"[0xdd06dbe3]
    (武器振り方向に対するサイズ::XL, 攻撃種類::コンボフィニッシュ) => "(コンボフィニッシュ, S)"[0x1a4a2df5]
    (武器振り方向に対するサイズ::XL, 攻撃種類::ジャンプ攻撃) => "(ジャンプ攻撃, S)"[0x2a5c3e5c]
    (武器振り方向に対するサイズ::XL, 攻撃種類::通常攻撃) => "(通常攻撃, S)"[0xf3c39b9e]
    (武器振り方向に対するサイズ::XL, 攻撃種類::特殊攻撃) => "(通常攻撃, S)"[0xf3c39b9e]
    (武器振り方向に対するサイズ::XL, 攻撃種類::溜め攻撃) => "(溜め攻撃, S)"[0x872a995d]
    (武器振り方向に対するサイズ::XL, 攻撃種類::溜め中攻撃) => "(溜め中攻撃, SandM)"[0xdd06dbe3]
  }
  Children {
    # omitted for brevity
  }
}
```
- Jump (EXKing and above)
  - Jumps to another call table outside of the current container
  - A jump container target cannot have >= 32 levels of child containers
```
Execute = Jump => SurpriseM[0x0d3388fe]
```

### Other Stuff I Need to Organize

```cpp
// other bits are entirely ignored
enum UserBitFlagSLink {
  IsNoPos = 1 << 0,
};

// TODO: the asset bit flags probably change by version (this is from totk)
enum AssetBitFlagELink {
  _00               = 1 << 0,
  IsFollow          = 1 << 1,
  IsUseOneEmitter   = 1 << 2,
  IsForceLoopAsset  = 1 << 3,
};

enum class AssetBitFlagSLink {
  _00                         = 1 << 0,
  IsNoParamUpdate             = 1 << 1,
  IsNoPos                     = 1 << 2,
  IsStopWhenEmitterDestroying = 1 << 3,
  IsUnified                   = 1 << 4,
  IsAutoOneTimeFade           = 1 << 5,
  IsForceLoopAsset            = 1 << 6,
};

// what to do when the user is clipped
enum ClipType {
  // older versions (pre-jump container)
  ClipNone    = 0,
  ClipKill    = 1,
  ClipReemit  = 2,
  ClipPause   = 3,

  // newer versions (post-jump container)
  ClipDefault = 0, // kill if not IsNeedObserve, otherwise reemit
  ClipKill    = 1,
  ClipReemit  = 2,
  ClipNone    = 3,
  ClipPause   = 4,
};

// TODO: check if these change by version (though probably not)
enum MtxSetType {
  IgnoreScale               = 0,  // scale only depends on the scale of the user
  Normal                    = 1,
  DefaultScale              = 2,
  DefaultRot                = 3,
  DefaultScaleRot           = 4,
  DefaultRotRotatePos       = 5,
  DefaultScaleRotRotatePos  = 6,
};

enum RotateSourceType {
  Param = 0,
  Model = 1,
};

// TODO: StartTimePosType

```

## Building

Requires CMake 3.18+ and gcc 16+
```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release # -G "Ninja" or whatever if you want
cmake --build build
```