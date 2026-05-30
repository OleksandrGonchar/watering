import { LOCAL_USER_ID, isLocalAuthMode } from "@/lib/authMode";

/**
 * Returns the current user id (Clerk user id, or `LOCAL_USER_ID` when
 * AUTH_MODE=local). Returns null only when Clerk is enabled and the visitor
 * is not signed in.
 */
export async function getCurrentUserId(): Promise<string | null> {
  if (isLocalAuthMode()) return LOCAL_USER_ID;
  const { auth } = await import("@clerk/nextjs/server");
  const { userId } = await auth();
  return userId;
}
