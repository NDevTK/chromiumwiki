# Chromium Security Wiki

This wiki contains analysis of Chromium components and potential security vulnerabilities.

**Important Note:** Research findings should be added directly to the relevant wiki page, following the format below. Do not create separate files for research notes. **Also, please keep the security research tips below very detailed; do not shorten them.**  **Additionally, please keep the wiki page list ordered by VRP risk (highest payout first).**

**Format of Each Wiki Page:**

Each wiki page follows a consistent format:

1.  **Component Focus:** The page clearly states the specific Chromium component(s) being analyzed (e.g., `content/browser/bluetooth/web_bluetooth_service_impl.cc`).

2.  **Potential Logic Flaws:** A list of potential logic flaws or vulnerabilities is provided, along with a brief description of each issue and its potential impact.

3.  **Further Analysis and Potential Issues:** This section provides a more detailed analysis of the identified issues, highlighting specific areas of concern within the codebase. Research notes, including files reviewed and key functions, are integrated here. This section also includes a summary of relevant CVEs and their connection to the discussed functionalities.

4.  **Code Analysis:** This section includes specific code snippets and observations.

5.  **Areas Requiring Further Investigation:** This section contains additional points for further investigation, identified during the analysis.

6.  **Secure Contexts and [Component Name]:** This section explains the interaction between the component's functionalities and secure contexts, highlighting the importance of secure contexts in mitigating vulnerabilities.

7.  **Privacy Implications:** This section discusses the privacy implications of the component's functionalities.

8.  **Additional Notes:** This section contains any additional relevant information or findings.

**Note:** The original `network.md` and `rendering_engine.md` files have been deleted, as their content has been split into more specific wikis. The `service_workers.md` file has been updated to remove payment-related content, which is now in `service_worker_payments.md`. The `security_headers.md` file has been split into more specific wiki pages for each security header.

**Tips for Security Researchers:**

Based on the Chromium Vulnerability Reward Program (VRP) data you provided, prioritize investigating files with high reward payouts, as these often indicate critical vulnerabilities. The data reveals several key areas for focused investigation:

*   **Site Isolation (`site_isolation.md`):** Thoroughly investigate the core components of site isolation, including `UrlInfo` (`url_info.md`), `SiteInfo` (`site_info.md`), `SiteInstance` (`site_instance.md`), `SiteInstanceGroup` (`site_instance_group.md`), and `RenderProcessHost` (`render_process_host.md`) classes, as well as the `NavigationRequest` (`navigation_request.md`) and `ChildProcessSecurityPolicyImpl` (`child_process_security_policy_impl.md`) classes, for potential logic errors that could compromise site isolation. Relevant files include: `render_process_host.md`, `url_info.md`, `site_info.md`, `site_instance.md`, `child_process_security_policy_impl.md`.

*   **Tab Management (`tabs.md`, `drag_and_drop.md`):**  The exceptionally high rewards for files like `tab_strip_model.cc` ($53,357) and `tabs_api.cc` ($14,604) highlight the criticality of tab management security. Focus your research on cross-origin communication, race conditions, and extension interactions. The drag-and-drop functionality within the tab strip also presents a high-risk area. Thoroughly analyze data validation, event handling, and cross-origin interactions to prevent injection attacks and data corruption. Relevant files include: `tab_strip_model.cc`, `tabs_api.cc`, `tab_drag_controller.cc`.

*   **Autofill (`autofill.md`):** The extremely high reward for `autofill_popup_controller_impl.cc` ($52,544) points to significant vulnerabilities in the autofill popup. Concentrate on data sanitization, data persistence, and form submission. Relevant files include: `autofill_popup_controller_impl.cc`, `chrome_autofill_client.cc`.

*   **Payments (`payments.md`):**  The substantial reward for `payment_request_sheet_controller.cc` ($16,326) indicates vulnerabilities in payment handling. Prioritize secure communication, data encryption, and data storage. Relevant files include: `payment_request_sheet_controller.cc`, `payment_request_dialog_view.cc`.

*   **Extensions (`extension_security.md`, `extensions_web_request_api.md`, `extensions_debugger_api.md`, `extensions_tabs_api.md`, `extensions_webrtc_audio_private_api.md`):**  High rewards for `tabs_api.cc` ($14,604) and `debugger_apitest.cc` ($15,309) highlight vulnerabilities in extension APIs and debugging. Focus on the permission model, sandbox bypasses, and API vulnerabilities. Also, pay close attention to the `extensions_web_request_api.md`, `extensions_debugger_api.md`, `extensions_tabs_api.md`, and `extensions_webrtc_audio_private_api.md` pages. Relevant files include: `tabs_api.cc`, `debugger_apitest.cc`, `extension_install_dialog_view.cc`, `extension_uninstall_dialog_view.cc`.

*   **WebRTC (`webrtc.md`):** The significant reward for `audio_debug_recordings_handler.cc` ($30,000) points to vulnerabilities in media handling. Focus on data stream integrity, media sanitization, and real-time communication security. Also, pay close attention to the `media_audio_debug_recordings_handler.md`, `media_web_contents_video_capture_device.md`, and `media_stream_dispatcher_host.md` pages. Relevant files include: `audio_debug_recordings_handler.cc`, `web_contents_display_observer_view.cc`, `media_router_dialog_controller_views.cc`, `tab_sharing_ui_views.cc`.

*   **Blink (`blink_layout.md`, `blink_frame.md`, `blink_core.md`):** The large number of rewarded files in the Blink components suggests a wide range of potential vulnerabilities. Focus on JavaScript execution, DOM manipulation, and cross-origin resource loading. Relevant files include: `html_canvas_element.cc`, `html_portal_element.cc`, `html_document_parser.cc`, `html_media_element.cc`, `html_iframe_element.cc`.

*   **Network (`disk_cache.md`, `quic.md`, `websockets.md`, `http.md`):**  The high reward for `network_context.h` ($16,000) suggests vulnerabilities in network handling. Focus on protocol handling, cookie handling, and caching mechanisms. Also, pay close attention to the `http.md` page. Relevant files include: `network_context.h`, `http_cache_transaction.cc`, `websocket_channel.h`.

*   **DevTools (`devtools.md`, `devtools_ui_bindings.md`):** The high rewards for `devtools_browsertest.cc` ($22,250) and `devtools_ui_bindings.cc` ($7,000) highlight the importance of secure DevTools implementation. Focus on unauthorized access, data leakage, command injection, and XSS vulnerabilities. Relevant files include: `devtools_browsertest.cc`, `devtools_ui_bindings.cc`.

*   **Permissions (`permissions.md`):** Investigate the permission model, focusing on how permissions are granted, stored, and enforced. Relevant files include: `permission_request_manager.cc`, `permission_prompt_impl.cc`.

*   **Web Authentication (`webauthn.md`):** Focus on the security of the Web Authentication API, including the handling of credentials and the interaction with authenticators. Relevant files include: `webauthn_bubble_view.cc`, `webauthn_icon_view.cc`.

*   **Media (`media.md`):** Investigate vulnerabilities related to media playback, encoding, and decoding. Focus on data stream integrity and media sanitization. Relevant files include: `media_stream_devices_controller.cc`, `media_stream_capture_indicator.cc`.

*   **WebAssembly (`wasm.md`):** Focus on the security of WebAssembly execution, including memory safety and control flow integrity. Relevant files include: `wasm-module.cc`, `wasm-compiler.cc`.

*   **User Notes (`user_notes.md`):** Investigate the security of user note storage and retrieval, focusing on data integrity and access control. Relevant files include: `user_note_service.cc`, `user_note_utils.cc`.

*   **Web Share (`webshare.md`):** Investigate the security of the Web Share API, focusing on data sanitization and origin handling. Relevant files include: `sharing_dialog_view.cc`, `share_operation.cc`.

*   **Web Bluetooth (`bluetooth.md`):** Focus on the security of the Web Bluetooth API, including device pairing and data transfer. Relevant files include: `web_bluetooth_service_impl.cc`, `bluetooth_chooser_controller.cc`.

*   **Web USB (`webusb.md`):** Investigate the security of the Web USB API, including device access and data transfer. Relevant files include: `usb_chooser_context.cc`, `usb_chooser_controller.cc`.

*   **Web Serial (`webserial.md`):** Focus on the security of the Web Serial API, including device access and data transfer. Relevant files include: `serial_port_underlying_sink.cc`, `serial.cc`.

*   **Web Codecs (`webcodecs.md`):** Investigate the security of the Web Codecs API, including encoding and decoding vulnerabilities. Relevant files include: `image_decoder_external.cc`, `video_encoder.cc`.

*   **WebGPU (`webgpu.md`):** Focus on the security of the WebGPU API, including memory management and shader execution. Relevant files include: `gpu_buffer.cc`, `gpu_device.cc`.

*   **WebXR (`webxr.md`):** Investigate the security of the WebXR API, including device access and data handling. Relevant files include: `xr_session.cc`, `xr_device.cc`.

*   **Payment Handler (`payment_handler.md`):** Focus on the security of the Payment Handler API, including secure communication and data handling. Relevant files include: `payment_handler_web_flow_view_controller.cc`, `payment_handler_modal_dialog_manager_delegate.cc`.

*   **Background Fetch (`background_fetch.md`):** Investigate the security of the Background Fetch API, including data persistence and resource management. Relevant files include: `background_fetch_job_controller.cc`, `background_fetch_delegate_proxy.cc`.

*   **Background Sync (`background_sync.md`):** Focus on the security of the Background Sync API, including data synchronization and resource management.

*   **Push Messaging (`push_messaging.md`):** Investigate the security of the Push Messaging API, including message handling and data security. Relevant files include: `push_messaging_manager.cc`.

*   **Storage (`storage.md`):** Focus on the security of storage mechanisms, including data persistence and access control. Relevant files include: `quota_manager_proxy.cc`, `indexed_db_backing_store.cc`.

*   **IndexedDB (`indexed_db.md`):** Investigate the security of IndexedDB, including data storage and access control. Relevant files include: `indexed_db_cursor.cc`, `indexed_db_transaction.cc`.

*  **File System Access (`file_system_access.md`):** Investigate the security of the File System Access API, including file access and data handling. Relevant files include: `file_system_access_manager_impl.cc`, `file_system_chooser.cc`.

*   **Native File System (`native_file_system.md`):** Focus on the security of the Native File System API, including file access and data handling. Relevant files include: `native_file_system_directory_access_confirmation_view.cc`, `native_file_system_directory_access_confirmation_view.h`.

*   **Downloads (`downloads.md`):** High rewards associated with download management highlight the importance of secure file handling and resource management. Focus on file type validation, download path sanitization, and resource leaks. Relevant files include: `download_manager_impl.cc`, `drag_download_file.cc`, `download_target_determiner.cc`.

*   **Printing (`printing.md`):** Investigate the security of printing functionality, including data handling and communication with print services. Relevant files include: `print_render_frame_helper.cc`, `print_web_view_helper.cc`.

*   **Accessibility (`accessibility.md`):** Focus on the security of accessibility features, including data handling and interaction with assistive technologies. Relevant files include: `ax_virtual_view.cc`, `ax_widget_obj_wrapper.cc`.

*   **Screen Capture (`screen_capture.md`):** Investigate the security of screen capture functionality, including data handling and access control. Relevant files include: `web_contents_video_capture_device.cc`, `desktop_capture_device.cc`.

*   **Screen Sharing (`screen_sharing.md`):** Focus on the security of screen sharing functionality, including data handling and access control. Relevant files include: `tab_sharing_ui_views.cc`, `tab_sharing_ui_views.h`.

*   **Cast (`cast.md`):** Investigate the security of Cast functionality, including device discovery and media streaming. Relevant files include: `cast_media_sink_service.cc`, `cast_app_discovery_service.cc`.

*   **Media Session (`media_session.md`):** Focus on the security of the Media Session API, including media playback and control. Relevant files include: `media_session_controller.cc`, `media_session_impl.cc`.

*   **Picture-in-Picture (`picture_in_picture.md`):** Investigate the security of Picture-in-Picture functionality, including window management and data handling. Relevant files include: `picture_in_picture_window_controller_impl.cc`, `document_picture_in_picture_window_controller_impl.cc`.
