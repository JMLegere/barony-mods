Feature: Launch Readiness Contract
  As the BaronyModLoader app-core launch boundary,
  I need semantic readiness, launch planning, and dry-run contracts,
  so the GUI can fail closed without starting Barony or claiming unsupported playability.

  Background:
    Given the BML Python app module path
    And a clean Launch Readiness staging directory

  @launch-readiness @readiness-matrix
  Scenario: Readiness matrix blocks every missing launch prerequisite
    When I build the launch readiness blocked matrix
    Then launch readiness reports install, profile, package, and runtime blockers

  @launch-readiness @launch-plan
  Scenario: Launch plan creates runtime manifest artifacts without launching
    When I create a launch plan manifest from hermetic fixtures
    Then the launch plan manifest and active mods artifacts are created
    And launch plan creation does not start a process

  @launch-readiness @stale-manifest
  Scenario: Stale manifest digests block launch planning
    When I plan launch readiness with stale manifest package digests
    Then stale manifest digest blockers are explicit
    And launch planning remains pure

  @launch-readiness @dry-run
  Scenario: Dry-run launch returns command metadata without starting Barony
    When I dry-run launch from hermetic fixtures
    Then the dry-run reports a launch command and manifest path
    And dry-run launch does not start a process

  @launch-readiness @windows-disabled
  Scenario: Windows launch is disabled until live Windows evidence exists
    When I evaluate Windows launch readiness without live runtime evidence
    Then Windows launch readiness is fail-closed and disabled

  @launch-readiness @playable-boundary
  Scenario: Playable claims are blocked for fake or scaffold-only runtime evidence
    When I evaluate playable launch claims from non-production evidence
    Then fake or scaffold-only evidence cannot be reported as playable
