Feature: Profile-first GUI button interactions
  As a BaronyModLoader user,
  I need visible GUI buttons to perform real, validated actions,
  so the profile-first launcher feels useful and trustworthy instead of inert.

  Background:
    Given a BaronyModLoader GUI button interaction smoke report path

  @gui-button-interactions @environment @safe-all
  Scenario: Environment safe all-click smoke updates readiness and diagnostics without launching Barony
    When I run the BaronyModLoader GUI with all smoke button clicks
    Then the button smoke invoked the safe Environment button actions
    And Environment button feedback updates readiness and diagnostics without starting Barony
    And unsafe all-click smoke does not include GUI launch actions


  @gui-button-interactions @copy-for-ai @safe-specific
  Scenario: Copy for AI smoke click invokes Tk button and reports pasteable issue-fix context without launching
    When I run the BaronyModLoader GUI with Copy for AI smoke button click
    Then the Copy for AI smoke invoked the Tk button without launching Barony
    And the Copy for AI smoke report exposes non-empty copied AI issue context metadata
    And the Copy for AI smoke report proves verified backend clipboard readback when available
    And copied AI issue context includes issue-fix essentials and rejects useless payloads

  @gui-button-interactions @copy-for-ai @wayland-discovery @safe-specific
  Scenario: Copy for AI smoke click writes text/plain when WAYLAND_DISPLAY is absent but a Wayland socket is discoverable
    When I run the BaronyModLoader GUI with Copy for AI smoke button click and no WAYLAND_DISPLAY
    Then the no-WAYLAND_DISPLAY Copy for AI smoke either gracefully skips on unavailable Wayland probing or records verified text/plain system clipboard readback

  @gui-button-interactions @copy-for-ai @wayland-discovery @stale-wayland-display @safe-specific
  Scenario: Copy for AI smoke click falls back from stale WAYLAND_DISPLAY to a discovered Wayland socket
    When I run the BaronyModLoader GUI with Copy for AI smoke button click and stale WAYLAND_DISPLAY
    Then the stale-WAYLAND_DISPLAY Copy for AI smoke either gracefully skips on unavailable Wayland probing or records verified text/plain system clipboard readback from a discovered fallback

  @gui-button-interactions @copy-for-ai @clipboard-fallback @safe-specific
  Scenario: Copy for AI smoke click writes a fallback file instead of claiming plain success when system readback fails
    When I run the BaronyModLoader GUI with Copy for AI smoke button click and system clipboard tools unavailable
    Then the Copy for AI smoke writes a fallback copy file and does not claim plain clipboard success

  @gui-button-interactions @environment @mocked-launch
  Scenario: Environment launch buttons start mocked BML and vanilla launches through Tk buttons
    When I run the BaronyModLoader GUI with mocked launch button clicks
    Then the mocked launch smoke invoked both GUI launch buttons through Tk
    And mocked launch feedback reports BML and Vanilla process launch metadata without starting Barony
    And mocked launch button metadata includes generated BML and vanilla Barony icon paths


  @gui-button-interactions @mocked-launch @multi-active-modlist
  Scenario: Launch BML Barony emits a multi-mod manifest when compatible packages are active
    When I run the BaronyModLoader GUI Launch BML smoke with Stash and Runebound active but Runebound selected
    Then the BML launch smoke accepts the compatible multi-active profile without a multiple-package blocker
    And the BML launch smoke writes a multi-mod runtime manifest for Stash and Runebound

  @gui-button-interactions @profiles
  Scenario: Profiles button creates and selects a stable profile outside .tmp
    When I run the BaronyModLoader GUI with all smoke button clicks
    Then the Profiles create-select-profile action creates and selects a stable profile outside .tmp

  @gui-button-interactions @mods @scan
  Scenario: Mods scan button lists Runebound
    When I run the BaronyModLoader GUI with all smoke button clicks
    Then the Mods scan-packages action lists Runebound: Elixirs

  @gui-button-interactions @mods @active-state
  Scenario: Mods enable and disable buttons mutate active mod state and are idempotent
    When I run the BaronyModLoader GUI with all smoke button clicks
    Then the Runebound enable and disable button clicks mutate active mod state
    And the Runebound enable and disable button clicks are idempotent

  @gui-button-interactions @workshop @dry-run
  Scenario: Workshop button produces a dry-run no-publish preview with no Steam side effects
    When I run the BaronyModLoader GUI with all smoke button clicks
    Then the Workshop preview action produces a dry-run no-publish preview
    And no Steam publish side effects are reported

  @gui-button-interactions @live-smoke
  Scenario: Live smoke invokes actual Tk buttons and reports clicked actions plus visible activity
    When I run the BaronyModLoader GUI with all smoke button clicks
    Then the smoke report proves actual Tk buttons were invoked
    And the smoke report includes clickedActions and a visible activity log for every expected button action

  @gui-button-interactions @playability-boundary
  Scenario: GUI button actions do not claim Runebound is playable
    When I run the BaronyModLoader GUI with all smoke button clicks
    Then no GUI button action claims Runebound: Elixirs is playable
