Feature: Tempfile / Side-Effect Guard for Dry-Run and Workshop Prep
  As a BML CI pipeline or user,
  I need dry-run and workshop-prep operations to be constrained to a
  caller-provided staging/profile directory,
  so no files are written to /tmp, the current directory, or any
  unintended location and no Steam publish/upload/auth commands are invoked.

  Background:
    Given the BML Python app module path
    And a temporary staging directory

  @bdd-harness
  Scenario: Package pack --dry-run writes output only to the specified --out path
    When I invoke the CLI command "package pack" in dry-run mode with --out to the staging dir
    Then the output archive is created at the specified --out path
    And the process exits with code 0

  @bdd-harness
  Scenario: Package install --dry-run does not write to the store directory
    When I invoke the CLI command "package install" in dry-run mode with --store
    Then the store directory is empty or does not contain the package
    And the process exits cleanly

  @bdd-harness
  Scenario: Workshop prep must not invoke Steam publish, upload, or auth commands
    When I invoke the CLI command for workshop prep
    Then no SteamCmd publish, upload, or auth subcommand appears in the invocation output
    And the process completes without calling the Steam web API

  @bdd-harness
  Scenario: Dry-run operations are constrained to the provided staging path
    When I run a Python script that invokes package operations with an explicit staging directory
    Then all file writes occur under the staging directory or are rejected
    And no writes occur outside the staging directory
