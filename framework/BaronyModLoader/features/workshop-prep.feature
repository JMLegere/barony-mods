Feature: Workshop Prep Contract
  As the BaronyModLoader GUI,
  I need a dry-run Workshop preparation service with semantic rows and reports,
  so maintainers can review packages locally without publishing or touching Steam.

  Background:
    Given the BML Python app module path
    And a clean Workshop Prep staging directory

  @workshop-prep @dry-run
  Scenario: Workshop safety invariant
    When I request the Workshop Prep "safety invariant" contract
    Then the Workshop Prep response is dry-run only and hidden by default

  @workshop-prep @selected-package
  Scenario: Selected package context
    When I request the Workshop Prep "selected package context" contract
    Then the Workshop Prep response identifies the selected Runebound package

  @workshop-prep @install-icons
  Scenario: Install context icons
    When I request the Workshop Prep "install context icons" contract
    Then the Workshop Prep response exposes install and Workshop icon labels

  @workshop-prep @metadata-validation
  Scenario: Metadata validation rows
    When I request the Workshop Prep "metadata validation rows" contract
    Then the Workshop Prep response contains semantic metadata validation rows

  @workshop-prep @preview-validation
  Scenario: Preview asset validation
    When I request the Workshop Prep "preview asset validation" contract
    Then the Workshop Prep response blocks placeholder or invalid preview assets

  @workshop-prep @local-staging
  Scenario: Local staging folder
    When I request the Workshop Prep "local staging folder" contract
    Then the Workshop Prep response stages only inside the local dry-run directory

  @workshop-prep @vdf-report
  Scenario: Dry-run VDF report
    When I request the Workshop Prep "dry-run VDF report" contract
    Then the Workshop Prep response includes a dry-run VDF report without publishing

  @workshop-prep @disabled-publish
  Scenario: Disabled publish invariant
    When I request the Workshop Prep "disabled publish invariant" contract
    Then the Workshop Prep response keeps publish disabled with explicit reasons

  @workshop-prep @no-steam-side-effects
  Scenario: No Steam side effects
    When I request the Workshop Prep "no Steam side effects" contract
    Then the Workshop Prep response reports no Steam, steamcmd, or network side effects
