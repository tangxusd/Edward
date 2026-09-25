export type CatalogType = "color" | "font";

export type CatalogManifest = {
  revision: string | null;
  fullHash: string | null;
  parentRevision?: string | null;
  delta: { items: unknown[]; removed: string[] };
  expiresAt?: string;
};

export type RedemptionResult = {
  redemptionId: string;
  entitlement: {
    currency: string;
    entitlementType: "subscription" | "credit";
    durationMonths: number | null;
    creditAmount: number | null;
    ruleVersion: number;
    effectiveAt: string;
    expiresAt: string;
  };
};

const sha256Pattern = /^[0-9a-f]{64}$/;

export function assertCatalogType(value: string): asserts value is CatalogType {
  if (value !== "color" && value !== "font") throw new Error("unsupported_catalog_type");
}

export function assertSha256(value: string): void {
  if (!sha256Pattern.test(value)) throw new Error("invalid_content_hash");
}

export function assertRequestId(value: string): void {
  if (!/^[A-Za-z0-9._:-]{8,128}$/.test(value)) throw new Error("invalid_request_id");
}

export function assertManifest(value: unknown): CatalogManifest {
  if (!value || typeof value !== "object") throw new Error("invalid_catalog_manifest");
  const manifest = value as Record<string, unknown>;
  if (manifest.revision !== null && typeof manifest.revision !== "string") throw new Error("invalid_catalog_revision");
  if (manifest.fullHash !== null && typeof manifest.fullHash !== "string") throw new Error("invalid_catalog_hash");
  if (!manifest.delta || typeof manifest.delta !== "object") throw new Error("invalid_catalog_delta");
  return value as CatalogManifest;
}
