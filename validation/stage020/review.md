# Focused read-only review

Scope: offline harness, independent coefficient oracle and diagnostic-profile
invariance. The requesting-code-review skill required a separate reviewer.

Important findings resolved:

- ASRC comparator originally assumed a linear within-block position. Native
  step slews across frames. Comparator now uses previous/current correction,
  sequential FP64 additions and zero-rebased origin; source absolute identity
  is retained separately. Gate1e-8 unchanged; production algorithm untouched.
- Silent windows and missing linear-channel coverage could produce vacuous
  PASS. Analyzer now requires excited actual/expected RMS and the exact seven
  intended lanes. Zero-window self-check reports BLOCKED SILENT WINDOW.

Review verified HP/LP/AP transfer equations, FIR partition reconstruction,
Thiran numerator/denominator and integer tap, legacy fractional delay, signed
gain and input trim. Current smallest expected band RMS≈1.61e-5.

Diagnostic crossover XML differs only in allPass1/2/3/5/6/7. Other stored
module blobs are identical. Matrix generation and Headroom revision differ
only as bookkeeping. Applied parameters/routes and manual Headroom gain match
the original export; Center/Sub/Scoop output maximum difference0.

ASRC oracle shares the actual coefficient bank and therefore checks scalar
arithmetic/indexing, not independent bank-design correctness. Virtual clock
and Stage2/current acoustic limitations remain explicit in the report.
