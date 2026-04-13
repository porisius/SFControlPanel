import { action } from "@elgato/streamdeck";
import { DescriptorActionBase } from "./descriptor-action-base";

@action({ UUID: "com.porisius.satisfactorycontrolpanel.customizer" })
export class CustomizerAction extends DescriptorActionBase {
  protected readonly suffix = "customizer" as const;
  protected readonly defaultTitle = "Customizer";
}
