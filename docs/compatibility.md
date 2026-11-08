# Compatibility policy (v0.3.0)

## User operation

Known and unknown runtimes use the same saved correction ON/OFF preference. The default is OFF. On unknown builds, ON requests the native observation and then activation after verification, without separate experiment buttons. A saved ON is also an ON request at startup. Turning OFF while waiting or observing prevents activation. Ordinary OFF/ON after activation updates the live profile without reattaching. Closing the UI expires the lease and requests restoration; it does not erase the saved preference.

## Gates retained

The native classifier still checks structure, readable and executable mappings, scalar entry shape, backend and all eight identities. The UI cannot override these checks. Controller readiness waits indefinitely, no more than one read-only census per second. A structural mismatch rejects immediately.

Once attached, observation forwards input unchanged. Within 15 seconds, all eight finger components must provide at least three valid successful calls, with stable receivers. Only native Verified permits native activation. Heartbeat, session, component identity and restoration checks remain in force. This is dynamic validation, not proof of support for every future SteamVR build.

The observation attachment is attempted only once per loaded vrserver session. A rejected or closed session is not silently reattached. Normal ON/OFF during a healthy session leaves the verified interception installed; OFF forwards values without correction. App exit or identity loss restores the slot. An unconfirmed restoration keeps correction disabled and requests a normal SteamVR restart.

## Evidence and limits

On 2026-11-08, the runtime with vrserver hash `fd0857d05ba7ecfd186c588c7b9412724a725ddc1aae90c79743efdb4b66a678` and cv hash `5ea5fcb5b104ca49d937b154c8567d232bcc0ec4887cc78134e5336f623b171d` was tested on one Frame. The full cv hash and logs are in testing.md. The unknown-build classifier and eight-finger observation passed; the user confirmed correction in VRChat. No profile for this build was added. The subsequent simplified UI flow is covered by deterministic tests; the new button flow and saved-ON startup were not separately validated live at packaging time. Long-duration stability and other devices remain unverified. Failed-restoration display must not be tested by corrupting a live vtable.

## Distribution

The packaged runtime enables structural audit and observation UI build flags. `--experimental` is required to install it on an unknown runtime. That flag permits registration only; it does not bypass runtime checks. No firmware, calibration or Valve installation files are changed.
