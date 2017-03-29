namespace cpp infra
namespace java infra.gen

enum Status {
	STATUS_OK = 0,
	STATUS_DUPLICATE,
	STATUS_DUPLICATE_KEY,
	STATUS_DUPLICATE_DATASPHERE,
	STATUS_DUPLICATE_NODE,
	STATUS_DUPLICATE_SERVICE,
	STATUS_DUPLICATE_VOLUME,

	STATUS_INVALID,
	STATUS_INVALID_KEY,
	STATUS_INVALID_VALUE,
	STATUS_INVALID_NAME,
	STATUS_INVALID_VOLUME,
	STATUS_INVALID_DATASPHERE,
	STATUS_INVALID_NODE,
	STATUS_INVALID_SERVICE,
	STATUS_INVALID_CREDENTIALS,
	STATUS_INVALID_TERM,
	STATUS_INVALID_RESOURCE,
	STATUS_INVALID_STATE,
	STATUS_INVALID_COMMIT,

	STATUS_IN_PROGRESS,
	STATUS_NOT_READY,
	STATUS_OFFLINE,
	STATUS_TRY_AGAIN,
	STATUS_PUBLISH_FAILED,
	STATUS_SUBSCRIBE_FAILED,
	STATUS_RESOURCE_LOAD_FAILED,
	STATUS_DB_WRITE_FAILURE,
	STATUS_DB_READ_FAILURE
	STATUS_NO_QUORUM,
	STATUS_NOT_LEADER
}
