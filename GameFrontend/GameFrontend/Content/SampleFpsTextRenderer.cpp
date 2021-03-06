﻿#include "pch.h"
#include "SampleFpsTextRenderer.h"

#include "Common/DirectXHelper.h"

using namespace GameFrontend;
using namespace Microsoft::WRL;

// Initializes D2D resources used for text rendering.
SampleFpsTextRenderer::SampleFpsTextRenderer(const std::shared_ptr<DX::DeviceResources>& deviceResources) : 
	m_text(L""),
	m_deviceResources(deviceResources)
{
	ZeroMemory(&m_textMetrics, sizeof(DWRITE_TEXT_METRICS));

	// Create device independent resources
	ComPtr<IDWriteTextFormat> textFormat;
	DX::ThrowIfFailed(
		m_deviceResources->GetDWriteFactory()->CreateTextFormat(
			L"Segoe UI",
			nullptr,
			DWRITE_FONT_WEIGHT_LIGHT,
			DWRITE_FONT_STYLE_NORMAL,
			DWRITE_FONT_STRETCH_NORMAL,
			32.0f,
			L"en-US",
			&textFormat
			)
		);

	DX::ThrowIfFailed(
		textFormat.As(&m_textFormat)
		);

	DX::ThrowIfFailed(
		m_textFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR)
		);

	DX::ThrowIfFailed(
		m_deviceResources->GetD2DFactory()->CreateDrawingStateBlock(&m_stateBlock)
		);

	CreateDeviceDependentResources();
}

// Updates the text to be displayed.
void SampleFpsTextRenderer::Update(DX::StepTimer const& timer)
{
	//// Update display text.
	//uint32 fps = timer.GetFramesPerSecond();

	//m_text = (fps > 0) ? std::to_wstring(fps) + L" FPS" : L" - FPS";

	if (!m_requestSent)
	{
		m_requestSent = true;

		// Update UI.
		m_text = L"Sending request...";

		// Get client ID from local settings storage.
		Platform::String^ clientId;

		Windows::Storage::ApplicationDataContainer^ localSettings =
			Windows::Storage::ApplicationData::Current->LocalSettings;

		if (localSettings->Values->HasKey("ClientId"))
		{
			// Lookup in local storage.
			clientId = localSettings->Values->Lookup("ClientId")->ToString();
		}
		else
		{
			// Create new client id.
			GUID guid;
			CoCreateGuid(&guid);

			OLECHAR* bstrGuid;
			StringFromCLSID(guid, &bstrGuid);

			clientId = ref new Platform::String(bstrGuid);

			// Store in local storage.
			localSettings->Values->Insert("ClientId", clientId);
		}

		// Send request.
		httpClient =
			ref new Windows::Web::Http::HttpClient();
		Windows::Foundation::Uri^ uri =
			ref new Windows::Foundation::Uri("http://localhost:8557/api/login/" + clientId);

		concurrency::create_task(httpClient->GetStringAsync(uri))
		.then([=](Platform::String^ s)
		{
			m_text = std::wstring(L"Response: ") + s->Data();
		})
		// Always catch network exceptions.
		.then([this](concurrency::task<void> t)
		{
			try
			{
				// Check for errors.
				t.get();
			}
			catch (Platform::Exception^ ex)
			{
				// Details in ex.Message and ex.HResult.
				m_text = std::wstring(L"Error: ") + ex->Message->Data();
			}
		});
	}


	ComPtr<IDWriteTextLayout> textLayout;
	DX::ThrowIfFailed(
		m_deviceResources->GetDWriteFactory()->CreateTextLayout(
			m_text.c_str(),
			(uint32) m_text.length(),
			m_textFormat.Get(),
			240.0f, // Max width of the input text.
			50.0f, // Max height of the input text.
			&textLayout
			)
		);

	DX::ThrowIfFailed(
		textLayout.As(&m_textLayout)
		);

	DX::ThrowIfFailed(
		m_textLayout->GetMetrics(&m_textMetrics)
		);
}

// Renders a frame to the screen.
void SampleFpsTextRenderer::Render()
{
	ID2D1DeviceContext* context = m_deviceResources->GetD2DDeviceContext();
	Windows::Foundation::Size logicalSize = m_deviceResources->GetLogicalSize();

	context->SaveDrawingState(m_stateBlock.Get());
	context->BeginDraw();

	// Position on the bottom right corner
	D2D1::Matrix3x2F screenTranslation = D2D1::Matrix3x2F::Translation(
		logicalSize.Width - m_textMetrics.layoutWidth,
		logicalSize.Height - m_textMetrics.height
		);

	context->SetTransform(screenTranslation * m_deviceResources->GetOrientationTransform2D());

	DX::ThrowIfFailed(
		m_textFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_TRAILING)
		);

	context->DrawTextLayout(
		D2D1::Point2F(0.f, 0.f),
		m_textLayout.Get(),
		m_whiteBrush.Get()
		);

	// Ignore D2DERR_RECREATE_TARGET here. This error indicates that the device
	// is lost. It will be handled during the next call to Present.
	HRESULT hr = context->EndDraw();
	if (hr != D2DERR_RECREATE_TARGET)
	{
		DX::ThrowIfFailed(hr);
	}

	context->RestoreDrawingState(m_stateBlock.Get());
}

void SampleFpsTextRenderer::CreateDeviceDependentResources()
{
	DX::ThrowIfFailed(
		m_deviceResources->GetD2DDeviceContext()->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White), &m_whiteBrush)
		);
}
void SampleFpsTextRenderer::ReleaseDeviceDependentResources()
{
	m_whiteBrush.Reset();
}