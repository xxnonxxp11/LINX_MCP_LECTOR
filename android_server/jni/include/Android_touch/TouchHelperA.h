#pragma once

extern bool other_touch;

bool Touch_Init(int w, int h, uint32_t orientation_, bool readOnly);
void UpdateScreenData(int w, int h, uint32_t orientation_);
void Touch_ApplyToImGui(); // llamar justo antes de ImGui::NewFrame() cada frame


void Touch_Close();
void Touch_Down(float x, float y);
void Touch_Move(float x, float y);
void Touch_Up();

