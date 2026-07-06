Feature: Core Runtime Manifest Planner Contract
  As the app/runtime boundary,
  I need a pure runtime manifest planner DTO,
  so the app can preview manifest path, manifest contents, stale digest blockers, and launch readiness without starting Barony.

  Background:
    Given the BML Python app module path
    And a clean Core Service Contract staging directory

  @core-service-contracts @runtime-manifest-planner
  Scenario: Runtime manifest planning is pure and projects stale digest blockers
    When I plan a runtime manifest with an intentionally stale package digest
    Then the manifest planner returns a DTO with output path and manifest payload
    And the planner projects stale digest blockers without launching a process
