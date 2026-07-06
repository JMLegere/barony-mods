Feature: Diagnostics BDD Cucumber Coverage
  As the BaronyModLoader app-core diagnostics layer,
  I need semantic runtime-report diagnostics that separate fake, degraded, stale, and production evidence,
  so the GUI can explain runtime state without launching Barony or scraping CLI text.

  Background:
    Given the BML Python app module path
    And a clean Diagnostics staging directory

  @diagnostics @app-core
  Scenario: Missing reports state
    When I load diagnostics for a missing runtime report
    Then diagnostics marks the report as missing with a fatal missing-report problem

  @diagnostics @app-core
  Scenario: Malformed report state
    When I load diagnostics for a malformed runtime report copy
    Then diagnostics marks the report as malformed with parse failure details

  @diagnostics @app-core
  Scenario: Runtime loaded summary
    When I load diagnostics for the loaded runtime report fixture
    Then diagnostics summarizes the loaded runtime report with loaded status and Runebound module evidence

  @diagnostics @app-core
  Scenario: Symbol missing degradation
    When I load diagnostics for a runtime report with a missing native symbol
    Then diagnostics degrades the report without claiming loaded runtime readiness

  @diagnostics @app-core
  Scenario: Stale report detection
    When I load diagnostics for a stale runtime report copy
    Then diagnostics flags the report as stale against the current app-core session

  @diagnostics @app-core
  Scenario: Production report parsing
    When I load diagnostics for a production-evidence runtime report copy
    Then diagnostics parses production evidence without requiring a live game launch
