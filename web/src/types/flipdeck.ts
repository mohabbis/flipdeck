/**
 * FlipDeck Action Types
 * Types for actions that can be sent via USB HID to connected computers
 */

export interface FlipDeckAction {
  label: string;
  type: "text" | "key" | "key_combo";
  value: string;
  confirm?: boolean;
  delay_ms?: number;
  description?: string;
  confirmation_required?: boolean;
}

export interface Profile {
  name: string;
  id?: string;
  description?: string;
  icon?: string;
  extends?: string;
  actions?: FlipDeckAction[];
  commands?: FlipDeckAction[];
  metadata?: {
    command_count: number;
    categories: string[];
    risk_count: number;
  };
}
