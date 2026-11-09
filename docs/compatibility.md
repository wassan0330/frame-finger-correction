# Compatibility policy (v0.3.1)

## User operation

Known and unknown runtimes use the same saved correction ON/OFF preference. The default is OFF. On unknown builds, ON requests the native observation and then activation after verification, without separate experiment buttons. A saved ON is also an ON request at startup. Turning OFF while waiting or observing prevents activation. Ordinary OFF/ON after activation updates the live profile without reattaching. Closing the UI expires the lease and requests restoration; it does not erase the saved preference.

## Gates retained

The native classifier still checks structure, readable and executable mappings, scalar entry shape, backend and all eight identities. The UI cannot override these checks. Controller readiness waits indefinitely, no more than one read-only census per second. A structural mismatch rejects immediately.

Once attached, the first real call for a recognized finger is forwarded with its value, receiver, component and offset unchanged. A successful finite normalized call verifies the common route. Native activation can then proceed according to the saved ON preference, without waiting for all eight fingers. Each individual finger is independently gated until its own successful raw call confirms its receiver. Constant input is sufficient: no value change, gesture, open-hand pose, or three-sample preparation is required. No synthetic call or replay is sent. The same call also primes the bank's receiver bookkeeping with correction and learning disabled, so the next real call can be corrected without another preparation sample. A silent finger does not delay others.

Structural census still identifies all eight finger components before attachment. Native heartbeat/session/identity checks, value/range and original-result checks, receiver consistency and restoration remain in force. Nothing is learned from this route verification; only the explicit LEARN operation establishes a new dynamic baseline.

The observation attachment is attempted only once per loaded vrserver session. A rejected or closed session is not silently reattached. Normal ON/OFF during a healthy session leaves the verified interception installed; OFF forwards values without correction. App exit or identity loss restores the slot. An unconfirmed restoration keeps correction disabled and requests a normal SteamVR restart.

## Evidence and limits

On 2026-11-08, the runtime with vrserver hash `fd0857d05ba7ecfd186c588c7b9412724a725ddc1aae90c79743efdb4b66a678` and cv hash `5ea5fcb5b104ca49d937b154c8567d232bcc0ec4887cc78134e5336f623b171d` was tested on one Frame. The full cv hash and logs are in testing.md. The unknown-build classifier and eight-finger observation passed; the user confirmed correction in VRChat. No profile for this build was added. The ordinary ON operation was subsequently confirmed in VRChat on public v0.3.0. On 2026-11-09, v0.3.1 route verification loaded in PID 14749 and reached fresh active 255 / native ON 255 255 0 from saved ON during ordinary use, with no requested finger preparation. This confirms activation and evidence collection, not a new visual-quality comparison. Constant-value/single-finger route behavior is verified in deterministic callback tests. Long-duration stability and other devices remain unverified. Failed-restoration display must not be tested by corrupting a live vtable.

## Distribution

The packaged runtime enables structural audit and observation UI build flags. `--experimental` is required to install it on an unknown runtime. That flag permits registration only; it does not bypass runtime checks. No firmware, calibration or Valve installation files are changed.
