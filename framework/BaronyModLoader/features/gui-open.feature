Feature: Visible GUI opening
  As a BaronyModLoader user,
  I need the gui command to open a real visible Tk dashboard,
  so the desktop launcher is validated beyond a readiness JSON response.

  Background:
    Given a hermetic GUI open smoke report path

  @gui-open @tk-check
  Scenario: Readiness check does not open a Tk root
    When I run the BaronyModLoader GUI command with arguments "--check"
    Then the GUI readiness check reports Tk availability
    And the GUI readiness check reports that no Tk root was opened

  @gui-open @tk-open
  Scenario: Auto-closing GUI opens a real Tk root and writes an opened report
    When I run the BaronyModLoader GUI command with auto-close smoke reporting
    Then the GUI command exits successfully
    And the GUI smoke report is written
    And the GUI smoke report says a Tk root was opened

  @gui-open @semantic-dashboard
  Scenario: Opened GUI report contains the merged Mods list and right-side cards
    When I run the BaronyModLoader GUI command with auto-close smoke reporting
    Then the GUI command exits successfully
    And the GUI smoke report contains the merged Mods list with Environment, Profiles, and Workshop cards

  @gui-open @blocked-status
  Scenario: Opened GUI report makes blocked actions and Workshop no-publish status visible
    When I run the BaronyModLoader GUI command with auto-close smoke reporting
    Then the GUI command exits successfully
    And the GUI smoke report shows disabled or blocking reasons
    And the GUI smoke report shows Workshop dry-run no-publish status

  @gui-open @playability-boundary
  Scenario: Opened GUI report does not claim Runebound is playable
    When I run the BaronyModLoader GUI command with auto-close smoke reporting
    Then the GUI command exits successfully
    And the GUI smoke report contains no visible Runebound playable claim
