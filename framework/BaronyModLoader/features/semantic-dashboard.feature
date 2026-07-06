Feature: Semantic Dashboard DTO Fixture Contract
  As a BML GUI dashboard or external integration,
  I need a structured dashboard data fixture with named sections
  (install, profile, package, readiness, diagnostics, workshop)
  so I can render a consistent UI without parsing unstructured stdout.

  Background:
    Given the BML Python app module path

  @bdd-harness
  Scenario: Dashboard DTO fixture exposes all required sections
    When I run a Python script that checks for a DashboardDto or equivalent fixture
    Then the module defines a dashboard fixture with sections: install, profile, package, readiness, diagnostics, workshop
    And each section is a dict or dataclass field

  @bdd-harness
  Scenario: Dashboard fixture includes disabled-reasons field
    When I run a Python script that checks for a DashboardDto
    Then the fixture includes a disabled_reasons field (list or None)
    And the disabled_reasons field is accessible as a property or key

  @bdd-harness
  Scenario: Dashboard fixture is plain dict or dataclass — no GUI types
    When I run a Python script that checks the type of the dashboard fixture
    Then the fixture is a dict or dataclass (not a tkinter widget or custom GUI object)
