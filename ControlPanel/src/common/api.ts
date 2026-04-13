export function normalizePort(value: unknown, fallback = 59384): number {
  if (typeof value === "number" && Number.isFinite(value)) return value;
  if (typeof value === "string") {
    const parsed = Number.parseInt(value, 10);
    if (Number.isFinite(parsed)) return parsed;
  }
  return fallback;
}

export function apiBase(port: number): string {
  return `http://localhost:${port}/api/v1`;
}

export function descriptorRoute(
  port: number,
  descriptor: string,
  suffix: "build" | "blueprint" | "execute" | "icon" | "customizer" | "scan" | "swatch"
): string {
  return `${apiBase(port)}/${encodeURIComponent(descriptor)}/${suffix}`;
}
