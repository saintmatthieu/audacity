# Flow
````mermaid
stateDiagram-v2
    DisambiguationButton: ÷2 or ×2 appears for 10s
    BiasedDetection: Consider current project tempo
    Detection: Ignore current project tempo
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
    isLoop --> useUnbiased : Project was empty ?
    useUnbiased --> Detection : yes
    Detection --> ClipY
    BiasedDetection --> ClipY
    ClipY --> isEmpty : "Hey, change project meter ?"
    isEmpty --> ProjY : yes
    isEmpty --> Disambiguation : no
    ProjY --> Disambiguation
    Disambiguation --> isAmbiguous : Was there another likely meter ?
    isAmbiguous --> [*] : no
    isAmbiguous --> DisambiguationButton : yes
    DisambiguationButton --> disambiguationUsed : Used ?
    disambiguationUsed --> [*] : no
    disambiguationUsed --> ClipZ : yes
    ClipZ --> isEmpty2 : Project was empty ?
    isEmpty2 --> [*] : no
    isEmpty2 --> ProjZ : yes
    ProjZ --> [*]
    useUnbiased --> BiasedDetection : no
````
