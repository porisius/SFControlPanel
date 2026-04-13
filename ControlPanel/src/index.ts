import streamDeck from "@elgato/streamdeck";
import { BuildableAction } from "./actions/buildable-action";
import { BlueprintAction } from "./actions/blueprint-action";
import { ExecuteAction } from "./actions/execute-action";
import { CustomizerAction } from "./actions/customizer-action";
// import { ResourceScanAction } from "./actions/resource-scan-action";
import { SwatchAction } from "./actions/swatch-action";

try {
  streamDeck.actions.registerAction(new BuildableAction());
  streamDeck.actions.registerAction(new BlueprintAction());
  streamDeck.actions.registerAction(new ExecuteAction());
  streamDeck.actions.registerAction(new CustomizerAction());
  // streamDeck.actions.registerAction(new ResourceScanAction()); // Temporarily disabled, until uPlugin supports the Resource Scanner
  streamDeck.actions.registerAction(new SwatchAction());
  streamDeck.connect();
} catch (error) {
  console.error("=== SCP startup crash ===", error);
}
