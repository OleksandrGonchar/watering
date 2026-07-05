import { AlertTriangle, Droplets } from "lucide-react";
import { cn } from "@/lib/utils";

type AlertVariant = "overflow" | "lowWater";

interface AlertBannerProps {
  variant: AlertVariant;
  title: string;
  description?: string;
}

const VARIANT_STYLES: Record<AlertVariant, string> = {
  overflow:
    "border-red-500/50 bg-red-500/10 text-red-700 dark:text-red-400",
  lowWater:
    "border-blue-500/50 bg-blue-500/10 text-blue-700 dark:text-blue-400",
};

export function AlertBanner({ variant, title, description }: AlertBannerProps) {
  const Icon = variant === "overflow" ? AlertTriangle : Droplets;

  return (
    <div
      role="alert"
      className={cn(
        "flex items-start gap-3 rounded-xl border p-4",
        VARIANT_STYLES[variant],
      )}
    >
      <Icon className="mt-0.5 h-5 w-5 shrink-0" />
      <div className="flex flex-col gap-0.5">
        <p className="text-sm font-semibold">{title}</p>
        {description ? (
          <p className="text-sm opacity-90">{description}</p>
        ) : null}
      </div>
    </div>
  );
}
