import { FlipDeckAction } from "@/types/flipdeck";

export function getActionTypeColor(type: FlipDeckAction["type"]): string {
  switch (type) {
    case "text":
      return "bg-blue-100 text-blue-800 dark:bg-blue-900 dark:text-blue-200";
    case "key":
      return "bg-green-100 text-green-800 dark:bg-green-900 dark:text-green-200";
    case "key_combo":
      return "bg-purple-100 text-purple-800 dark:bg-purple-900 dark:text-purple-200";
    default:
      return "bg-gray-100 text-gray-800";
  }
}
