import { action } from "@elgato/streamdeck";
import { DescriptorActionBase } from "./descriptor-action-base";

@action({ UUID: "com.porisius.satisfactorycontrolpanel.blueprint" })
export class BlueprintAction extends DescriptorActionBase {
  protected readonly suffix = "blueprint" as const;
  protected readonly defaultTitle = "Blueprint";
}
