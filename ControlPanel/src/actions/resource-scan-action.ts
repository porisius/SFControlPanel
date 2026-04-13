import { action } from "@elgato/streamdeck";
import { DescriptorActionBase } from "./descriptor-action-base";

@action({ UUID: "com.porisius.satisfactorycontrolpanel.resourcescan" })
export class ResourceScanAction extends DescriptorActionBase {
  protected readonly suffix = "scan" as const;
  protected readonly defaultTitle = "Scan";
}
