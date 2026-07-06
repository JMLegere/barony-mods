Feature: Core Profile Store Contract
  As the one-page BaronyModLoader app,
  I need profile state to live in a stable profile-local store,
  so install, package, readiness, diagnostics, and workshop services all project the same active-mod state without using product .tmp paths.

  Background:
    Given the BML Python app module path
    And a clean Core Service Contract staging directory

  @core-service-contracts @profile-store
  Scenario: Stable profile store creates, loads, and projects active mods
    When I exercise the profile store contract with one enabled Runebound package
    Then the profile store returns stable profile paths without a product .tmp path
    And the active mods projection contains enabled package DTOs
