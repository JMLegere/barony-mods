Feature: Core Diagnostics Repository Contract
  As the app diagnostics service,
  I need structured runtime report diagnostics and evidence classification,
  so fake-provider evidence is never presented as Linux production evidence and broken reports surface semantic errors.

  Background:
    Given the BML Python app module path
    And a clean Core Service Contract staging directory

  @core-service-contracts @diagnostics-repository
  Scenario: Diagnostics repository parses broken reports and classifies evidence
    When I load diagnostics for missing, malformed, fake, and production evidence reports
    Then the diagnostics repository reports missing and malformed runtime reports
    And fake-provider evidence is classified separately from real Linux production evidence
