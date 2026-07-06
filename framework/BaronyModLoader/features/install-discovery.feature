Feature: Install Discovery App-Core Contract
  As the one-page BaronyModLoader GUI,
  I need app-core to expose semantic Barony install state,
  so the GUI can render install readiness without parsing raw CLI or Steam output.

  Background:
    Given the BML Python app module path
    And a clean Install Discovery staging directory

  @install-discovery @app-core-contract
  Scenario: No install state fails closed with semantic disabled reasons
    Given no Barony install exists in the discovery inputs
    When I ask app-core to build the install discovery state
    Then the install discovery state is missing and disables install-dependent actions
    And the install discovery state exposes OS, store, and runtime status icons

  @install-discovery @app-core-contract
  Scenario: Linux Steam verified install is projected as a semantic install state
    Given a Linux Steam library contains a verified Barony install with build id "123456"
    When I ask app-core to build the install discovery state
    Then the install discovery state includes a verified Linux Steam install with path and build evidence
    And the install discovery state exposes OS, store, and runtime status icons

  @install-discovery @app-core-contract
  Scenario: Windows Steam install is visible but fail-closed until live verification
    Given a Windows Steam discovery record exists without live verification evidence
    When I ask app-core to build the install discovery state
    Then the install discovery state lists Windows but keeps it fail-closed
    And Windows install disabled reasons require live verification

  @install-discovery @app-core-contract
  Scenario: Discovery searches multiple Steam libraries instead of stopping at the first library
    Given multiple Steam libraries exist and only the secondary library contains Barony build id "654321"
    When I ask app-core to build the install discovery state
    Then the install discovery state includes the Barony install from the secondary Steam library

  @install-discovery @app-core-contract
  Scenario: Malformed discovery output becomes a structured disabled install state
    Given Steam discovery returns malformed install output
    When I ask app-core to build the install discovery state
    Then the malformed discovery output is reported as a structured disabled install state

  @install-discovery @app-core-contract
  Scenario: Selected install propagates into semantic app-core state
    Given a Linux Steam install is selected for the active app-core session
    When I ask app-core to build the install discovery state
    Then the selected install propagates to the dashboard and readiness state
