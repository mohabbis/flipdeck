/**
 * FlipDeck Action Types
 * Types for actions that can be sent via USB HID to connected computers
 */

export interface FlipDeckAction {
  label: string;
  type: "text" | "key" | "key_combo";
  value: string;
  confirm: boolean;
}

export interface Profile {
  name: string;
  id: string;
  description: string;
  actions: FlipDeckAction[];
}