import { action } from "@elgato/streamdeck";
import { DescriptorActionBase } from "./descriptor-action-base";

@action({ UUID: "com.porisius.satisfactorycontrolpanel.buildable" })
export class BuildableAction extends DescriptorActionBase {
  protected readonly suffix = "build" as const;
  protected readonly defaultTitle = "Build";
}
