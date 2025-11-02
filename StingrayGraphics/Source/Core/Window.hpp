#pragma once

typedef int SRWindowFlags;

enum SRWindowFlags_ {
	SRWindowFlags_None = 0,
	SRWindowFlags_Centered = 1 << 0,
	SRWindowFlags_SizeIsClientArea = 1 << 1
};

class SRWindow {
public:
	SRWindow(const char* name, int width, int height, SRWindowFlags flags = SRWindowFlags_None);
	~SRWindow();

	bool poll_events();
	void show();

	void* get_internal_handle() const;
	void get_client_size(int* width, int* height) const;
	float get_client_aspect_ratio() const;

private:
	struct Impl;
	Impl* m_Impl;
};
