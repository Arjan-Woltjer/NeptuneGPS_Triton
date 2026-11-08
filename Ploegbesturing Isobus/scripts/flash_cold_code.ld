		/* AgIsoStack code that runs at boot, at a pool upload, a VT or TC
		   (re)connect, or on a rare request: run it from flash, like FLASHMEM.
		   Inserted after the core's *(.flashmem*) by flash_cold_code.py; see
		   there for why. Each function is its own section (-ffunction-sections),
		   named .text.<mangled name>; _ZN*6isobus covers _ZN and _ZNK (const).
		   A pattern that stops matching (a rename upstream) only puts that
		   function back in ITCM: check teensy_size after a library bump. */

		/* VT client: construction, the pool scaling and upload, AUX-N
		   assignment handling, the version commands */
		*(.text._ZN6isobus21VirtualTerminalClientC[12]E*)
		*(.text._ZN6isobus21VirtualTerminalClientD[012]E*)
		*(.text._ZN*6isobus21VirtualTerminalClient*initialize*)
		*(.text._ZN*6isobus21VirtualTerminalClient4stop*)
		*(.text._ZN*6isobus21VirtualTerminalClient*set_object_pool*)
		*(.text._ZN*6isobus21VirtualTerminalClient*scale_object_pools*)
		*(.text._ZN*6isobus21VirtualTerminalClient*resize_object*)
		*(.text._ZN*6isobus21VirtualTerminalClient*remap_font_to_scale*)
		*(.text._ZN*6isobus21VirtualTerminalClient*scale_proportional_font_height*)
		*(.text._ZN*6isobus21VirtualTerminalClient*get_soft_key_scaled_object_ids*)
		*(.text._ZN*6isobus21VirtualTerminalClient*get_number_bytes_in_object*)
		*(.text._ZN*6isobus21VirtualTerminalClient*get_minimum_object_length*)
		*(.text._ZN*6isobus21VirtualTerminalClient*print_objectpool_error*)
		*(.text._ZN*6isobus21VirtualTerminalClient*process_internal_object_pool_upload_callback*)
		*(.text._ZN*6isobus21VirtualTerminalClient*prepare_auxiliary_assignment_transaction_locked*)
		*(.text._ZN*6isobus21VirtualTerminalClient*apply_auxiliary_assignment_command_locked*)
		*(.text._ZN*6isobus21VirtualTerminalClient*handle_auxiliary_assignment_command*)
		*(.text._ZN*6isobus21VirtualTerminalClient*ensure_auxiliary_preferences_loaded*)
		*(.text._ZN*6isobus21VirtualTerminalClient*erase_auxiliary_function_assignment*)
		*(.text._ZN*6isobus21VirtualTerminalClient*store_pending_auxiliary_preferences*)
		*(.text._ZN*6isobus21VirtualTerminalClient*send_store_version*)
		*(.text._ZN*6isobus21VirtualTerminalClient*send_load_version*)
		*(.text._ZN*6isobus21VirtualTerminalClient*send_delete_version*)

		/* TC: the device descriptor object pool and its objects, built at boot
		   and serialised when a TC asks for the DDOP */
		*(.text._ZN*6isobus26DeviceDescriptorObjectPool*)
		*(.text._ZN*6isobus22task_controller_object*)

		/* Diagnostics: start and stop, and the identification replies */
		*(.text._ZN6isobus18DiagnosticProtocolC[12]E*)
		*(.text._ZN6isobus18DiagnosticProtocolD[012]E*)
		*(.text._ZN*6isobus18DiagnosticProtocol*initialize*)
		*(.text._ZN*6isobus18DiagnosticProtocol*terminate*)
		*(.text._ZN*6isobus18DiagnosticProtocol*send_software_identification*)
		*(.text._ZN*6isobus18DiagnosticProtocol*send_product_identification*)
		*(.text._ZN*6isobus18DiagnosticProtocol*send_ecu_identification*)
		*(.text._ZN*6isobus18DiagnosticProtocol*send_diagnostic_protocol_identification*)
