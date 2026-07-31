## What changed

<!-- The defect or need, then what this does about it. -->

## Why

<!-- What goes wrong without this. For a fix, how it is reached. -->

## Validation

<!-- What you ran and what it showed. Name anything you could NOT verify --
     no hardware, no toolchain, untested path -- rather than leaving it implied. -->

- [ ] Native tests pass for affected projects (`pio run -e native`, then run the binary)
- [ ] Affected firmware still builds (`pio run -e <board env>`)
- [ ] Tested on hardware, or explicitly noted below as not tested

## Conventions

- [ ] Follows `NeptuneGPS Documentation/Conventions/` for the languages touched
- [ ] New or substantially reworked files carry the canonical header from `FILE_HEADERS.md`
- [ ] Tests added or updated where behaviour changed

## Risk

<!-- This firmware moves implements and applies chemical. If the change affects
     when an actuator or pump runs, say so here. -->
