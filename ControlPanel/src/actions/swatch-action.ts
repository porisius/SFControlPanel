import {
  SingletonAction,
  type DidReceiveSettingsEvent,
  type KeyDownEvent,
  type WillAppearEvent
} from "@elgato/streamdeck";
import { action } from "@elgato/streamdeck";
import { descriptorRoute, normalizePort } from "../common/api";

export interface SwatchSettings {
  [key: string]: unknown;
  buttonLabel?: string;
  port?: number;
  descriptor?: string;
  friendlyName?: string;
  primaryColor?: string;
  secondaryColor?: string;
}

@action({ UUID: "com.porisius.satisfactorycontrolpanel.swatch" })
export class SwatchAction extends SingletonAction<SwatchSettings> {
  protected readonly defaultTitle = "Swatch";

  protected normalizeCssColor(value: string, fallback: string): string {
    const trimmed = value.trim();

    if (!trimmed) {
      return fallback;
    }

    if (trimmed.startsWith("#")) {
      const noHash = trimmed.slice(1);

      if (/^[0-9a-fA-F]{8}$/.test(noHash)) {
        return `#${noHash.slice(0, 6)}`;
      }

      if (/^[0-9a-fA-F]{6}$/.test(noHash)) {
        return `#${noHash}`;
      }

      return fallback;
    }

    if (/^[0-9a-fA-F]{8}$/.test(trimmed)) {
      return `#${trimmed.slice(0, 6)}`;
    }

    if (/^[0-9a-fA-F]{6}$/.test(trimmed)) {
      return `#${trimmed}`;
    }

    return fallback;
  }

  protected normalize(settings: SwatchSettings) {
    return {
      port: normalizePort(settings.port),
      descriptor: typeof settings.descriptor === "string" ? settings.descriptor : "",
      buttonLabel:
        typeof settings.buttonLabel === "string"
          ? settings.buttonLabel.replace(/\\n/g, "\n")
          : "",
      friendlyName: typeof settings.friendlyName === "string" ? settings.friendlyName : "",
      primaryColor: this.normalizeCssColor(
        typeof settings.primaryColor === "string" ? settings.primaryColor : "",
        "#000000"
      ),
      secondaryColor: this.normalizeCssColor(
        typeof settings.secondaryColor === "string" ? settings.secondaryColor : "",
        "#FFFFFF"
      )
    };
  }

  protected generateTwoColorSvgDataUrl(primaryColor: string, secondaryColor: string): string {
    const size = 144;
    const center = 72;
    const radius = 70;
    const innerRadius = 60;

    const svg = `
<svg xmlns="http://www.w3.org/2000/svg" width="${size}" height="${size}" viewBox="0 0 ${size} ${size}">
  <defs>
    <clipPath id="innerClip">
      <circle cx="${center}" cy="${center}" r="${innerRadius}" />
    </clipPath>
  </defs>
  <circle cx="${center}" cy="${center}" r="${radius}" fill="#808080" />
  <g clip-path="url(#innerClip)">
    <polygon points="0,0 ${size},0 0,${size}" fill="${primaryColor}" />
    <polygon points="${size},0 ${size},${size} 0,${size}" fill="${secondaryColor}" />
  </g>
  <circle cx="${center}" cy="${center}" r="${innerRadius}" fill="none" stroke="rgba(255,255,255,0.18)" stroke-width="1" />
</svg>`.trim();

    return `data:image/svg+xml;charset=utf-8,${encodeURIComponent(svg)}`;
  }

  protected async applyVisuals(
    actionRef:
      | DidReceiveSettingsEvent<SwatchSettings>["action"]
      | WillAppearEvent<SwatchSettings>["action"]
      | KeyDownEvent<SwatchSettings>["action"],
    settings: SwatchSettings
  ): Promise<void> {
    const s = this.normalize(settings);

    if (s.buttonLabel) {
      await actionRef.setTitle(s.buttonLabel);
    } else {
      await actionRef.setTitle("");
    }

    const icon = this.generateTwoColorSvgDataUrl(s.primaryColor, s.secondaryColor);
    await actionRef.setImage(icon);
  }

  override async onDidReceiveSettings(ev: DidReceiveSettingsEvent<SwatchSettings>): Promise<void> {
    await this.applyVisuals(ev.action, ev.payload.settings);
  }

  override async onWillAppear(ev: WillAppearEvent<SwatchSettings>): Promise<void> {
    await this.applyVisuals(ev.action, ev.payload.settings);
  }

  override async onKeyDown(ev: KeyDownEvent<SwatchSettings>): Promise<void> {
    const s = this.normalize(ev.payload.settings);

    if (!s.descriptor) {
      await ev.action.showAlert();
      return;
    }

    try {
      const response = await fetch(descriptorRoute(s.port, s.descriptor, "swatch"), {
        method: "GET"
      });

      if (!response.ok) {
        console.error("[swatch] request failed", response.status, await response.text());
        await ev.action.showAlert();
        return;
      }

      await this.applyVisuals(ev.action, ev.payload.settings);
      await ev.action.showOk();
    } catch (error) {
      console.error("[swatch] request threw", error);
      await ev.action.showAlert();
    }
  }
}