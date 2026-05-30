/**
 * Authentication mode toggle.
 *
 * - `clerk` (default) — uses Clerk for user auth. Requires `pk_test_…` /
 *   `sk_test_…` env vars and an internet connection.
 * - `local` — bypass Clerk entirely. Every request is treated as if it came
 *   from a single user with id `LOCAL_USER_ID`. Useful for offline dev,
 *   demos, or single-user self-hosted deployments.
 *
 * Switch via `AUTH_MODE=local` in `.env`.
 */

export const LOCAL_USER_ID = "local-user";

export function isLocalAuthMode(): boolean {
  return process.env.AUTH_MODE === "local";
}
