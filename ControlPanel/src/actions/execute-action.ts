import { action } from "@elgato/streamdeck";
import { DescriptorActionBase } from "./descriptor-action-base";

@action({ UUID: "com.porisius.satisfactorycontrolpanel.execute" })
export class ExecuteAction extends DescriptorActionBase {
  protected readonly suffix = "execute" as const;
  protected readonly defaultTitle = "Action";
}
