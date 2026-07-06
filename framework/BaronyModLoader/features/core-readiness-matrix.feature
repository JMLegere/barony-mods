Feature: Core Readiness Matrix Contract
  As the one-page BaronyModLoader app,
  I need a pure readiness matrix with blockers and disabled reasons,
  so every launch/workshop action can fail closed when install, profile, package, or runtime evidence is not ready.

  Background:
    Given the BML Python app module path
    And a clean Core Service Contract staging directory

  @core-service-contracts @readiness-matrix
  Scenario: Readiness matrix blocks missing inputs and unverified Windows runtime
    When I build readiness matrices for missing inputs and unverified Windows runtime
    Then the readiness matrix exposes rows, blockers, and disabled reasons
    And missing install, profile, package, and Windows fail-closed blockers are explicit
