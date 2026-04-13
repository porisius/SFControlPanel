import {
  SingletonAction,
  type DidReceiveSettingsEvent,
  type KeyDownEvent,
  type WillAppearEvent
} from "@elgato/streamdeck";
import { descriptorRoute, normalizePort } from "../common/api";

export type DescriptorSuffix = "build" | "blueprint" | "execute" | "customizer" | "scan";

export interface DescriptorSettings {
  [key: string]: unknown;
  buttonLabel?: string;
  port?: number;
  descriptor?: string;
  friendlyName?: string;
}

export abstract class DescriptorActionBase extends SingletonAction<DescriptorSettings> {
  protected abstract readonly suffix: DescriptorSuffix;
  protected abstract readonly defaultTitle: string;

  protected normalize(settings: DescriptorSettings) {
    return {
      port: normalizePort(settings.port),
      descriptor: typeof settings.descriptor === "string" ? settings.descriptor : "",
      buttonLabel:
        typeof settings.buttonLabel === "string"
          ? settings.buttonLabel.replace(/\\n/g, "\n")
          : "",
      friendlyName: typeof settings.friendlyName === "string" ? settings.friendlyName : ""
    };
  }

  protected async fetchIconAsDataUrl(port: number, descriptor: string): Promise<string | undefined> {
    try {
      const response = await fetch(descriptorRoute(port, descriptor, "icon"), { method: "GET" });
      if (!response.ok) {
        console.error(`[${this.suffix}] icon fetch failed`, response.status, await response.text());
        return undefined;
      }

      const contentType = response.headers.get("content-type") || "image/png";
      const arrayBuffer = await response.arrayBuffer();
      const base64 = Buffer.from(arrayBuffer).toString("base64");
      return `data:${contentType};base64,${base64}`;
    } catch (error) {
      console.error(`[${this.suffix}] icon fetch threw`, error);
      return undefined;
    }
  }

  protected async applyVisuals(
    action:
      | DidReceiveSettingsEvent<DescriptorSettings>["action"]
      | WillAppearEvent<DescriptorSettings>["action"]
      | KeyDownEvent<DescriptorSettings>["action"],
    settings: DescriptorSettings
  ): Promise<void> {
    const s = this.normalize(settings);

    if (s.buttonLabel) {
      await action.setTitle(s.buttonLabel);
    } else {
      await action.setTitle("");
    }

    if (s.descriptor) {
      const dataUrl = await this.fetchIconAsDataUrl(s.port, s.descriptor);
      if (dataUrl) {
        await action.setImage(dataUrl);
      }
    }
  }

  override async onDidReceiveSettings(ev: DidReceiveSettingsEvent<DescriptorSettings>): Promise<void> {
    await this.applyVisuals(ev.action, ev.payload.settings);
  }

  override async onWillAppear(ev: WillAppearEvent<DescriptorSettings>): Promise<void> {
    await this.applyVisuals(ev.action, ev.payload.settings);
  }

  override async onKeyDown(ev: KeyDownEvent<DescriptorSettings>): Promise<void> {
    const s = this.normalize(ev.payload.settings);

    if (!s.descriptor) {
      await ev.action.showAlert();
      return;
    }

    try {
      const response = await fetch(descriptorRoute(s.port, s.descriptor, this.suffix), { method: "GET" });
      if (!response.ok) {
        console.error(`[${this.suffix}] request failed`, response.status, await response.text());
        await ev.action.showAlert();
        return;
      }

      await this.applyVisuals(ev.action, ev.payload.settings);
      await ev.action.showOk();
    } catch (error) {
      console.error(`[${this.suffix}] request threw`, error);
      await ev.action.showAlert();
    }
  }
}
