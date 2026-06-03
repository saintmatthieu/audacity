```plantuml
@startuml
actor User
participant App as "App\n\n(examples:\n* Audacity\n* test host\n* 3rd-party host ?\n* ...)"
participant Host as "AudiocomCloudEffectHost\n(CLAP host)"
participant CloudEffect as "CloudEffect\n(CLAP plugin)"
participant "IAu3AudioComService\n(wraps audio.com REST)" as audiocom

== User has selected audio and opens effect ==
User ->> App: openCloudEffect(id)
App ->> Host: openCloudEffect(id)
note over Host, CloudEffect: CLAP stuff
User ->> App: apply
App ->> Host: apply(selection)
Host -[#DarkOrange]>> audiocom: **uploadCloudEffectJob(selection, params, ...)**
note over Host, audiocom #FEE8C8: NEW interface — both on the host-facing\nIAu3AudioComService and on the audio.com REST side.\nBundles upload + effect id + settings into one job\n(today's share/upload methods can't express this).
== progress updates ==
audiocom -->> Host: ProgressPtr (upload progress)
Host -->> App: ProgressPtr (upload progress)
note over App: toast:\n"Uploading..."
audiocom -->> Host: ProgressPtr (processing progress)
Host -->> App: ProgressPtr (processing progress)
note over App: toast:\n"Cloud effect XYZ in progress..."
...
audiocom -->> Host: ProgressPtr (download progress)
Host -->> App: ProgressPtr (download progress)
note over App: toast:\n"Downloading..."
loop download tracks
Host ->> audiocom: downloadAudioFile(audioId)
end
...

== Job complete ==
Host -->> App: onComplete(track1, track2, labels1, ...)
App -->> App: emplace tracks
note over App: toast:\n"New tracks ready"
Host -[#DarkOrange]>> audiocom: **clearCloudEffectJob(jobId)**
@enduml

@startuml Cloud-effect flow
actor User
participant Audacity
participant CloudEffect as "CloudEffect\n(frontend)"
participant Audiocom
participant AudiocomCloudEffect as "AudiocomCloudEffect\n(backend)"
participant 3rd as "3rdPartyCloudEffect\n(backend)\n(other server)"

User -> CloudEffect:open
activate CloudEffect
note over CloudEffect:UI
User -> CloudEffect: apply
CloudEffect -->> Audacity:toast("Cloud effect initiated...")
CloudEffect -->> Audiocom: process(\n\tprojectId,\n\tselection,\n\teffectId,\n\texportFormat,\n\tparams)
deactivate CloudEffect
note over Audiocom:Creates audio from selection\nusing audacit exec:\n`audacity project -export selection exportFormat`
Audiocom -->> Audacity:toast("cloud processing....")
Audiocom -> Audiocom: resolve effect id
alt is audiocom effect
Audiocom -->> AudiocomCloudEffect:process(inputUrls, params)
...
...
AudiocomCloudEffect -->> Audiocom: outputs
else is 3rd-party effect
note over Audiocom: resolve effect id -> vetted provider\n(registered endpoint + auth,\nfrom audio.com's provider registry)
Audiocom ->> Audiocom: mint signed input URLs (selection)
Audiocom ->> 3rd: POST {provider.endpoint}/job\n{ inputUrls, params, callbackUrl, jobToken }
3rd -->> Audiocom: 202 Accepted { jobId }

note over 3rd:processing ...
...
...
note over 3rd:... done.\nuploads outputs (audio + labels + ...)
3rd ->> Audiocom: POST {callbackUrl}\n{ jobToken, outputs: [\n  {type:"audio",  url, name, startTime},\n  {type:"labels", url, format},\n  ... ] }
note over Audiocom: verify jobToken
end
note over Audiocom:`audacity project -integrate selection outputs`\n(dispatch by type: audio -> track, labels -> label track)
Audiocom -->> Audacity:sync
@enduml

@startuml CloudEffectExtension
class CloudEffectExtensionModel {
  Q_INVOKABLE void apply()
}
note top of CloudEffectExtensionModel: `apply()` collects project ID, selection, ...\nand calls IAu3AudioComService

class CloudEffectExtension {
  +required property string effectId
  +property var params // JSON-serializable
  +function apply()
}
note top of CloudEffectExtension: Has access to all Muse UI Components\n(`import MuseApi.Controls`)

class MyCloudEffectExtension {}
note bottom of MyCloudEffectExtension:* provide UI\n* provide effect ID for audiocom server\n* optionally sets `params`

CloudEffectExtension <|-- MyCloudEffectExtension
CloudEffectExtension *-- CloudEffectExtensionModel
@enduml

@startuml

@enduml
```
