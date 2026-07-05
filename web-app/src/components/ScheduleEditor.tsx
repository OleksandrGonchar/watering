"use client";

import { useState } from "react";
import { useRouter } from "next/navigation";
import { Trash2, Plus } from "lucide-react";
import { Button } from "@/components/ui/button";
import { Input } from "@/components/ui/input";
import { Label } from "@/components/ui/label";

const MAX_SCHEDULES = 5;
const DEFAULT_MAX_DURATION = 600;

type ScheduleRow = {
  timeLocal: string;
  durationSeconds: number;
};

interface ScheduleEditorProps {
  deviceId: string;
  initialSchedules: ScheduleRow[];
  // Which actuator this editor manages. Sent to the API so it replaces only
  // this type's rows. Defaults to watering to stay backwards-compatible.
  type?: "watering" | "humidifier";
  maxDuration?: number;
  emptyHint?: string;
}

export function ScheduleEditor({
  deviceId,
  initialSchedules,
  type = "watering",
  maxDuration = DEFAULT_MAX_DURATION,
  emptyHint,
}: ScheduleEditorProps) {
  const router = useRouter();
  const [rows, setRows] = useState<ScheduleRow[]>(initialSchedules);
  const [saving, setSaving] = useState(false);
  const [message, setMessage] = useState<
    { type: "ok" | "err"; text: string } | null
  >(null);

  function addRow() {
    if (rows.length >= MAX_SCHEDULES) return;
    setRows([...rows, { timeLocal: "09:00", durationSeconds: 15 }]);
  }

  function updateRow(idx: number, patch: Partial<ScheduleRow>) {
    setRows((prev) => prev.map((r, i) => (i === idx ? { ...r, ...patch } : r)));
  }

  function removeRow(idx: number) {
    setRows((prev) => prev.filter((_, i) => i !== idx));
  }

  async function save() {
    setSaving(true);
    setMessage(null);
    try {
      const res = await fetch(`/api/devices/${deviceId}/schedules`, {
        method: "PUT",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ type, schedules: rows }),
      });
      if (!res.ok) {
        const data = await res.json().catch(() => ({}));
        setMessage({
          type: "err",
          text:
            (data as { error?: string }).error ?? "Не вдалося зберегти розклад",
        });
        return;
      }
      setMessage({ type: "ok", text: "Розклад збережено." });
      router.refresh();
    } catch {
      setMessage({ type: "err", text: "Не вдалося зв'язатися із сервером." });
    } finally {
      setSaving(false);
    }
  }

  return (
    <div className="flex flex-col gap-4">
      <div className="flex items-center justify-between">
        <p className="text-sm text-muted-foreground">
          До {MAX_SCHEDULES} розкладів. Тривалість — від 1 до {maxDuration}{" "}
          секунд.
        </p>
        <Button
          type="button"
          variant="outline"
          size="sm"
          onClick={addRow}
          disabled={rows.length >= MAX_SCHEDULES}
        >
          <Plus className="mr-1 h-4 w-4" />
          Додати
        </Button>
      </div>

      {rows.length === 0 ? (
        <div className="rounded-md border border-dashed p-6 text-center text-sm text-muted-foreground">
          {emptyHint ?? "Немає розкладів — пристрій не поливатиме."}
        </div>
      ) : (
        <div className="flex flex-col gap-3">
          {rows.map((r, idx) => (
            <div
              key={idx}
              className="flex items-end gap-3 rounded-md border p-3"
            >
              <div className="flex flex-col gap-1">
                <Label htmlFor={`time-${idx}`}>Час</Label>
                <Input
                  id={`time-${idx}`}
                  type="time"
                  value={r.timeLocal}
                  onChange={(e) =>
                    updateRow(idx, { timeLocal: e.target.value })
                  }
                  className="w-32"
                />
              </div>
              <div className="flex flex-col gap-1">
                <Label htmlFor={`dur-${idx}`}>Тривалість (сек)</Label>
                <Input
                  id={`dur-${idx}`}
                  type="number"
                  min={1}
                  max={maxDuration}
                  value={r.durationSeconds}
                  onChange={(e) =>
                    updateRow(idx, {
                      durationSeconds: Math.max(
                        1,
                        Math.min(maxDuration, Number(e.target.value) || 0),
                      ),
                    })
                  }
                  className="w-32"
                />
              </div>
              <Button
                type="button"
                variant="ghost"
                size="icon"
                onClick={() => removeRow(idx)}
                aria-label="Видалити"
              >
                <Trash2 className="h-4 w-4" />
              </Button>
            </div>
          ))}
        </div>
      )}

      {message && (
        <p
          className={
            message.type === "ok"
              ? "rounded-md bg-green-100 p-2 text-sm text-green-900"
              : "rounded-md bg-destructive/10 p-2 text-sm text-destructive"
          }
        >
          {message.text}
        </p>
      )}

      <div className="flex justify-end">
        <Button onClick={save} disabled={saving}>
          {saving ? "Збереження…" : "Зберегти"}
        </Button>
      </div>
    </div>
  );
}
