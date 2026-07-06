Feature: Core Facade Journey Contract
  As the one-page BaronyModLoader app shell,
  I need one semantic dashboard object built from profile, package, readiness, diagnostics, and workshop services,
  so GUI widgets can render app state without owning product logic or parsing command output.

  Background:
    Given the BML Python app module path
    And a clean Core Service Contract staging directory

  @core-service-contracts @core-facade
  Scenario: Core facade returns one semantic dashboard object for the dry-run journey
    When I request the core facade dashboard for a dry-run app journey
    Then the facade returns one semantic dashboard object with all core sections
    And the facade dashboard preserves workshop stub state and disabled reasons without raw stdout
