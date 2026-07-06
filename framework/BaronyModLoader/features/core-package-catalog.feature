Feature: Core Package Catalog Contract
  As the one-page BaronyModLoader app,
  I need a package catalog service that scans local mods and returns semantic package summaries,
  so the UI can show valid and invalid packages without scraping validation stdout.

  Background:
    Given the BML Python app module path
    And a clean Core Service Contract staging directory

  @core-service-contracts @package-catalog
  Scenario: Package catalog scans local mods and summarizes validation state
    When I ask the package catalog service to scan local mods with a malformed fixture
    Then the catalog contains semantic summaries for valid and invalid packages
    And the catalog includes the Runebound package validation status without raw stdout parsing
