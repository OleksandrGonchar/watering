"use client";

import { useState } from "react";
import { useRouter } from "next/navigation";
import { Button } from "@/components/ui/button";
import { Input } from "@/components/ui/input";
import { Label } from "@/components/ui/label";
import { Card, CardContent, CardHeader, CardTitle } from "@/components/ui/card";
import { COMMON_TIMEZONES } from "@/lib/timezone";

export default function ClaimDevicePage() {
  const router = useRouter();
  const [deviceId, setDeviceId] = useState("");
  const [claimCode, setClaimCode] = useState("");
  const [name, setName] = useState("");
  const [timezone, setTimezone] = useState("Europe/Kyiv");
  const [submitting, setSubmitting] = useState(false);
  const [error, setError] = useState<string | null>(null);

  async function onSubmit(e: React.FormEvent) {
    e.preventDefault();
    setSubmitting(true);
    setError(null);

    try {
      const res = await fetch("/api/devices/claim", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({
          deviceId: deviceId.trim(),
          claimCode: claimCode.trim(),
          name: name.trim() || undefined,
          timezone,
        }),
      });

      if (!res.ok) {
        const data = await res.json().catch(() => ({}));
        const code = (data as { error?: string }).error;
        const messages: Record<string, string> = {
          device_not_found: "Пристрій із таким ID не знайдено.",
          already_claimed: "Цей пристрій вже прив'язано до іншого акаунта.",
          device_not_claimable: "Пристрій уже прив'язано — claim_code анульований.",
          invalid_claim_code: "Невірний CLAIM_CODE.",
          invalid_timezone: "Невідома таймзона.",
          unauthorized: "Потрібен логін.",
        };
        setError(messages[code ?? ""] ?? "Помилка при прив'язці пристрою.");
        return;
      }

      router.push(`/dashboard/device/${deviceId.trim()}`);
      router.refresh();
    } catch {
      setError("Не вдалося зв'язатися із сервером.");
    } finally {
      setSubmitting(false);
    }
  }

  return (
    <div className="mx-auto max-w-md">
      <Card>
        <CardHeader>
          <CardTitle>Додати новий пристрій</CardTitle>
        </CardHeader>
        <CardContent>
          <form onSubmit={onSubmit} className="flex flex-col gap-4">
            <div className="flex flex-col gap-1.5">
              <Label htmlFor="deviceId">DEVICE_ID</Label>
              <Input
                id="deviceId"
                value={deviceId}
                onChange={(e) => setDeviceId(e.target.value)}
                placeholder="ESP_BEDROOM_FLOWERS_01"
                required
              />
            </div>

            <div className="flex flex-col gap-1.5">
              <Label htmlFor="claimCode">CLAIM_CODE</Label>
              <Input
                id="claimCode"
                value={claimCode}
                onChange={(e) => setClaimCode(e.target.value)}
                placeholder="A1B2C3"
                required
              />
            </div>

            <div className="flex flex-col gap-1.5">
              <Label htmlFor="name">Назва (опційно)</Label>
              <Input
                id="name"
                value={name}
                onChange={(e) => setName(e.target.value)}
                placeholder="Фікус на підвіконні"
              />
            </div>

            <div className="flex flex-col gap-1.5">
              <Label htmlFor="tz">Таймзона</Label>
              <select
                id="tz"
                value={timezone}
                onChange={(e) => setTimezone(e.target.value)}
                className="h-9 w-full rounded-md border border-input bg-transparent px-3 py-1 text-sm shadow-sm"
              >
                {COMMON_TIMEZONES.map((tz) => (
                  <option key={tz} value={tz}>
                    {tz}
                  </option>
                ))}
              </select>
            </div>

            {error && (
              <p className="rounded-md bg-destructive/10 p-3 text-sm text-destructive">
                {error}
              </p>
            )}

            <div className="flex justify-end gap-2">
              <Button
                type="button"
                variant="outline"
                onClick={() => router.push("/dashboard")}
              >
                Скасувати
              </Button>
              <Button type="submit" disabled={submitting}>
                {submitting ? "Прив'язую…" : "Прив'язати"}
              </Button>
            </div>
          </form>
        </CardContent>
      </Card>
    </div>
  );
}
