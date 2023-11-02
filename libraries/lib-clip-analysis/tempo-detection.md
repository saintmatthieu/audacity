# Flow
````mermaid
stateDiagram-v2
    DisambiguationButton: ÷2 or ×2 appears for 10s
    BiasedDetection: Biased Detection
    Detection: Unbiased Detection
    ProjX: Project Meter X
    ProjY: Project Meter Y
    ProjZ: Project Meter Z
    ClipY: Clip Meter Y
    ClipZ: Clip Meter Z
    state isLoop <<choice>>
    state useUnbiased <<choice>>
    state isAmbiguous <<choice>>
    state disambiguationUsed <<choice>>
    state isEmpty <<choice>>
    state isEmpty2 <<choice>>
    [*] --> ProjX
    ProjX --> Import
    Import --> isLoop: Is loop AND Beats-and-Bars view ON ?
    isLoop --> [*]: no
    isLoop --> useUnbiased : Project is empty ?
    useUnbiased --> Detection : yes
    Detection --> ClipY
    BiasedDetection --> ClipY
    ClipY --> isEmpty : Project empty ?
    isEmpty --> ProjY : yes
    isEmpty --> Disambiguation : no
    ProjY --> Disambiguation
    Disambiguation --> isAmbiguous : Could have been Z ?
    isAmbiguous --> [*] : no
    isAmbiguous --> DisambiguationButton : yes
    DisambiguationButton --> disambiguationUsed : Used ?
    disambiguationUsed --> [*] : no
    disambiguationUsed --> ClipZ
    ClipZ --> isEmpty2 : Project is empty ?
    isEmpty2 --> [*] : no
    isEmpty2 --> ProjZ : yes
    ProjZ --> [*]
    useUnbiased --> BiasedDetection : no
````
