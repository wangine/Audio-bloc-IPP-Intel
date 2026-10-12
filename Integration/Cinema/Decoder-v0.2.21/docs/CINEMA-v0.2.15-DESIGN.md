# Cinema v0.2.15: retain deadlines without coarse Windows waits

The live Cinema path drops into source-wait even while decoded PCM remains
available. The v0.2.14 decoder pacer and publication worker round short waits
to the Windows scheduler tick, then emit bursts. Do not compensate with gain,
filters, SRC changes or larger queues.

The bounded correction uses one precreated high-resolution waitable timer per
owner thread. Existing pacer deadlines (128 frames, existing queue correction)
and publication idle delay (1 ms) remain identical. Timer creation is validated
before starting the worker. A wait failure retires the publication generation;
it must not spin and fabricate successful output. Existing invalidation,
recovery, 48 kHz legacy ring, WDL filter, channel order and GUI controls remain.

Keep v0.2.14 immutable. Build v0.2.15 separately. Verify the isolated committed
PCM cadence regression, existing numerical/transport/retirement checks, then
observe actual Cinema with the existing DAC profile. Stage 1/2 are unchanged.
The separate Stage 3 r3 candidate fixes startup references to unpublished PCM;
it does not make coarse source delivery correct by itself.

Acceptance requires local live evidence. No Git upload before stability and
the user's listening confirmation. Preserve timing traces and candidate logs.
