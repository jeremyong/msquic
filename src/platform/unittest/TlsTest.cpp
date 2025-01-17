/*++

    Copyright (c) Microsoft Corporation.
    Licensed under the MIT License.

--*/

#define _CRT_SECURE_NO_WARNINGS 1
#include "main.h"
#include "msquic.h"
#include "quic_tls.h"
#ifdef _WIN32
#include <wincrypt.h>
#endif
#include <fcntl.h>

#ifdef QUIC_CLOG
#include "TlsTest.cpp.clog.h"
#endif

const uint32_t DefaultFragmentSize = 1200;

const uint8_t Alpn[] = { 1, 'A' };
const uint8_t MultiAlpn[] = { 1, 'C', 1, 'A', 1, 'B' };
const char* PfxPass = "PLACEHOLDER";        // approved for cred scan
extern const char* PfxPath;

struct TlsTest : public ::testing::TestWithParam<bool>
{
protected:
    CXPLAT_SEC_CONFIG* ServerSecConfig {nullptr};
    CXPLAT_SEC_CONFIG* ServerSecConfigClientAuth {nullptr};
    CXPLAT_SEC_CONFIG* ServerSecConfigDeferClientAuth {nullptr};
    CXPLAT_SEC_CONFIG* ClientSecConfig {nullptr};
    CXPLAT_SEC_CONFIG* ClientSecConfigDeferredCertValidation {nullptr};
    CXPLAT_SEC_CONFIG* ClientSecConfigCustomCertValidation {nullptr};
    CXPLAT_SEC_CONFIG* ClientSecConfigExtraCertValidation {nullptr};
    CXPLAT_SEC_CONFIG* ClientSecConfigNoCertValidation {nullptr};
    CXPLAT_SEC_CONFIG* ClientSecConfigClientCertNoCertValidation {nullptr};
    CXPLAT_SEC_CONFIG* Pkcs12SecConfig {nullptr};
    static QUIC_CREDENTIAL_FLAGS SelfSignedCertParamsFlags;
    static QUIC_CREDENTIAL_CONFIG* SelfSignedCertParams;
    static QUIC_CREDENTIAL_CONFIG* ClientCertParams;
    static QUIC_CREDENTIAL_CONFIG* CertParamsFromFile;

    TlsTest() { }

    ~TlsTest()
    {
        TearDown();
    }

    _Function_class_(CXPLAT_SEC_CONFIG_CREATE_COMPLETE)
    static void
    QUIC_API
    OnSecConfigCreateComplete(
        _In_ const QUIC_CREDENTIAL_CONFIG* /* CredConfig */,
        _In_opt_ void* Context,
        _In_ QUIC_STATUS Status,
        _In_opt_ CXPLAT_SEC_CONFIG* SecConfig
        )
    {
        VERIFY_QUIC_SUCCESS(Status);
        ASSERT_NE(nullptr, SecConfig);
        *(CXPLAT_SEC_CONFIG**)Context = SecConfig;
    }

#ifndef QUIC_DISABLE_PFX_TESTS
    static uint8_t* ReadFile(const char* Name, uint32_t* Length) {
        size_t FileSize = 0;
        FILE* Handle = fopen(Name, "rb");
        if (Handle == NULL) {
            return NULL;
        }
#ifdef _WIN32
        struct _stat Stat;
        if (_fstat(_fileno(Handle), &Stat) == 0) {
            FileSize = (int)Stat.st_size;
        }
#else
        struct stat Stat;
        if (fstat(fileno(Handle), &Stat) == 0) {
            FileSize = (int)Stat.st_size;
        }
#endif
        if (FileSize == 0) {
            fclose(Handle);
            return NULL;
        }

        uint8_t* Buffer = (uint8_t *)CXPLAT_ALLOC_NONPAGED(FileSize, QUIC_POOL_TEST);
        if (Buffer == NULL) {
            fclose(Handle);
            return NULL;
        }

        size_t ReadLength = 0;
        *Length = 0;
        do {
            ReadLength = fread(Buffer + *Length, 1, FileSize - *Length, Handle);
            *Length += (uint32_t)ReadLength;
        } while (ReadLength > 0 && *Length < (uint32_t)FileSize);
        fclose(Handle);
        if (*Length != FileSize) {
            CXPLAT_FREE(Buffer, QUIC_POOL_TEST);
            return NULL;
        }
        return Buffer;
    }
#endif

    static void SetUpTestSuite()
    {
        SelfSignedCertParams = (QUIC_CREDENTIAL_CONFIG*)CxPlatGetSelfSignedCert(CXPLAT_SELF_SIGN_CERT_USER, FALSE);
        ASSERT_NE(nullptr, SelfSignedCertParams);
        SelfSignedCertParamsFlags = SelfSignedCertParams->Flags;
#ifndef QUIC_DISABLE_CLIENT_CERT_TESTS
        ClientCertParams = (QUIC_CREDENTIAL_CONFIG*)CxPlatGetSelfSignedCert(CXPLAT_SELF_SIGN_CERT_USER, TRUE);
        ASSERT_NE(nullptr, ClientCertParams);
        ClientCertParams->Flags |= QUIC_CREDENTIAL_FLAG_NO_CERTIFICATE_VALIDATION;
#endif
#ifndef QUIC_DISABLE_PFX_TESTS
        ASSERT_NE(nullptr, PfxPath);
        CertParamsFromFile = (QUIC_CREDENTIAL_CONFIG*)CXPLAT_ALLOC_NONPAGED(sizeof(QUIC_CREDENTIAL_CONFIG), QUIC_POOL_TEST);
        ASSERT_NE(nullptr, CertParamsFromFile);
        CxPlatZeroMemory(CertParamsFromFile, sizeof(*CertParamsFromFile));
        CertParamsFromFile->Type = QUIC_CREDENTIAL_TYPE_CERTIFICATE_PKCS12;
        CertParamsFromFile->CertificatePkcs12 = (QUIC_CERTIFICATE_PKCS12*)CXPLAT_ALLOC_NONPAGED(sizeof(QUIC_CERTIFICATE_PKCS12), QUIC_POOL_TEST);
        ASSERT_NE(nullptr, CertParamsFromFile->CertificatePkcs12);
        CxPlatZeroMemory(CertParamsFromFile->CertificatePkcs12, sizeof(QUIC_CERTIFICATE_PKCS12));
        CertParamsFromFile->CertificatePkcs12->Asn1Blob = ReadFile(PfxPath, &CertParamsFromFile->CertificatePkcs12->Asn1BlobLength);
        CertParamsFromFile->CertificatePkcs12->PrivateKeyPassword = PfxPass;
        ASSERT_NE((uint32_t)0, CertParamsFromFile->CertificatePkcs12->Asn1BlobLength);
        ASSERT_NE(nullptr, CertParamsFromFile->CertificatePkcs12->Asn1Blob);
#endif
    }

    static void TearDownTestSuite()
    {
        CxPlatFreeSelfSignedCert(SelfSignedCertParams);
        SelfSignedCertParams = nullptr;
#ifndef QUIC_DISABLE_CLIENT_CERT_TESTS
        CxPlatFreeSelfSignedCert(ClientCertParams);
        ClientCertParams = nullptr;
#endif
#ifndef QUIC_DISABLE_PFX_TESTS
        if (CertParamsFromFile->CertificatePkcs12->Asn1Blob) {
            CXPLAT_FREE(CertParamsFromFile->CertificatePkcs12->Asn1Blob, QUIC_POOL_TEST);
        }
        CXPLAT_FREE(CertParamsFromFile->CertificatePkcs12, QUIC_POOL_TEST);
        CXPLAT_FREE(CertParamsFromFile, QUIC_POOL_TEST);
        CertParamsFromFile = nullptr;
#endif
    }

    void SetUp() override
    {
        SelfSignedCertParams->Flags = SelfSignedCertParamsFlags; // Make sure to start fresh
        VERIFY_QUIC_SUCCESS(
            CxPlatTlsSecConfigCreate(
                SelfSignedCertParams,
                CXPLAT_TLS_CREDENTIAL_FLAG_NONE,
                &TlsContext::TlsServerCallbacks,
                &ServerSecConfig,
                OnSecConfigCreateComplete));
        ASSERT_NE(nullptr, ServerSecConfig);

#ifndef QUIC_DISABLE_CLIENT_CERT_TESTS
        SelfSignedCertParams->Flags = SelfSignedCertParamsFlags | QUIC_CREDENTIAL_FLAG_REQUIRE_CLIENT_AUTHENTICATION;
        VERIFY_QUIC_SUCCESS(
            CxPlatTlsSecConfigCreate(
                SelfSignedCertParams,
                CXPLAT_TLS_CREDENTIAL_FLAG_NONE,
                &TlsContext::TlsServerCallbacks,
                &ServerSecConfigClientAuth,
                OnSecConfigCreateComplete));
        ASSERT_NE(nullptr, ServerSecConfigClientAuth);

        SelfSignedCertParams->Flags =
            QUIC_CREDENTIAL_FLAG_REQUIRE_CLIENT_AUTHENTICATION | QUIC_CREDENTIAL_FLAG_DEFER_CERTIFICATE_VALIDATION |
            QUIC_CREDENTIAL_FLAG_INDICATE_CERTIFICATE_RECEIVED;
        VERIFY_QUIC_SUCCESS(
            CxPlatTlsSecConfigCreate(
                SelfSignedCertParams,
                CXPLAT_TLS_CREDENTIAL_FLAG_NONE,
                &TlsContext::TlsServerCallbacks,
                &ServerSecConfigDeferClientAuth,
                OnSecConfigCreateComplete));
        ASSERT_NE(nullptr, ServerSecConfigDeferClientAuth);
#endif

        QUIC_CREDENTIAL_CONFIG ClientCredConfig = {
            QUIC_CREDENTIAL_TYPE_NONE,
            QUIC_CREDENTIAL_FLAG_CLIENT,
            NULL,
            NULL,
            NULL,
            NULL
        };
        VERIFY_QUIC_SUCCESS(
            CxPlatTlsSecConfigCreate(
                &ClientCredConfig,
                CXPLAT_TLS_CREDENTIAL_FLAG_NONE,
                &TlsContext::TlsClientCallbacks,
                &ClientSecConfig,
                OnSecConfigCreateComplete));
        ASSERT_NE(nullptr, ClientSecConfig);

        ClientCredConfig.Flags =
            QUIC_CREDENTIAL_FLAG_CLIENT | QUIC_CREDENTIAL_FLAG_INDICATE_CERTIFICATE_RECEIVED | QUIC_CREDENTIAL_FLAG_DEFER_CERTIFICATE_VALIDATION;
        CxPlatTlsSecConfigCreate( // Don't assert as this is expected to fail on some platforms
            &ClientCredConfig,
            CXPLAT_TLS_CREDENTIAL_FLAG_NONE,
            &TlsContext::TlsClientCallbacks,
            &ClientSecConfigDeferredCertValidation,
            OnSecConfigCreateComplete);

        ClientCredConfig.Flags =
            QUIC_CREDENTIAL_FLAG_CLIENT | QUIC_CREDENTIAL_FLAG_NO_CERTIFICATE_VALIDATION | QUIC_CREDENTIAL_FLAG_INDICATE_CERTIFICATE_RECEIVED;
        VERIFY_QUIC_SUCCESS(
            CxPlatTlsSecConfigCreate(
                &ClientCredConfig,
                CXPLAT_TLS_CREDENTIAL_FLAG_NONE,
                &TlsContext::TlsClientCallbacks,
                &ClientSecConfigCustomCertValidation,
                OnSecConfigCreateComplete));
        ASSERT_NE(nullptr, ClientSecConfigCustomCertValidation);

        ClientCredConfig.Flags =
            QUIC_CREDENTIAL_FLAG_CLIENT | QUIC_CREDENTIAL_FLAG_INDICATE_CERTIFICATE_RECEIVED;
        VERIFY_QUIC_SUCCESS(
            CxPlatTlsSecConfigCreate(
                &ClientCredConfig,
                CXPLAT_TLS_CREDENTIAL_FLAG_NONE,
                &TlsContext::TlsClientCallbacks,
                &ClientSecConfigExtraCertValidation,
                OnSecConfigCreateComplete));
        ASSERT_NE(nullptr, ClientSecConfigExtraCertValidation);

        ClientCredConfig.Flags =
            QUIC_CREDENTIAL_FLAG_CLIENT | QUIC_CREDENTIAL_FLAG_NO_CERTIFICATE_VALIDATION;
        VERIFY_QUIC_SUCCESS(
            CxPlatTlsSecConfigCreate(
                &ClientCredConfig,
                CXPLAT_TLS_CREDENTIAL_FLAG_NONE,
                &TlsContext::TlsClientCallbacks,
                &ClientSecConfigNoCertValidation,
                OnSecConfigCreateComplete));
        ASSERT_NE(nullptr, ClientSecConfigNoCertValidation);

#ifndef QUIC_DISABLE_CLIENT_CERT_TESTS
        VERIFY_QUIC_SUCCESS(
            CxPlatTlsSecConfigCreate(
                ClientCertParams,
                CXPLAT_TLS_CREDENTIAL_FLAG_NONE,
                &TlsContext::TlsClientCallbacks,
                &ClientSecConfigClientCertNoCertValidation,
                OnSecConfigCreateComplete));
        ASSERT_NE(nullptr, ClientSecConfigClientCertNoCertValidation);
#endif
#ifndef QUIC_DISABLE_PFX_TESTS
        ASSERT_NE(nullptr, CertParamsFromFile);
        VERIFY_QUIC_SUCCESS(
            CxPlatTlsSecConfigCreate(
                CertParamsFromFile,
                CXPLAT_TLS_CREDENTIAL_FLAG_NONE,
                &TlsContext::TlsClientCallbacks,
                &Pkcs12SecConfig,
                OnSecConfigCreateComplete));
        ASSERT_NE(nullptr, Pkcs12SecConfig);
#endif
    }

    void TearDown() override
    {
        if (ClientSecConfigNoCertValidation) {
            CxPlatTlsSecConfigDelete(ClientSecConfigNoCertValidation);
            ClientSecConfigNoCertValidation = nullptr;
        }
        if (ClientSecConfigExtraCertValidation) {
            CxPlatTlsSecConfigDelete(ClientSecConfigExtraCertValidation);
            ClientSecConfigExtraCertValidation = nullptr;
        }
        if (ClientSecConfigCustomCertValidation) {
            CxPlatTlsSecConfigDelete(ClientSecConfigCustomCertValidation);
            ClientSecConfigCustomCertValidation = nullptr;
        }
        if (ClientSecConfigDeferredCertValidation) {
            CxPlatTlsSecConfigDelete(ClientSecConfigDeferredCertValidation);
            ClientSecConfigDeferredCertValidation = nullptr;
        }
        if (ClientSecConfigClientCertNoCertValidation) {
            CxPlatTlsSecConfigDelete(ClientSecConfigClientCertNoCertValidation);
            ClientSecConfigClientCertNoCertValidation  = nullptr;
        }
        if (ClientSecConfig) {
            CxPlatTlsSecConfigDelete(ClientSecConfig);
            ClientSecConfig = nullptr;
        }
        if (ServerSecConfigClientAuth) {
            CxPlatTlsSecConfigDelete(ServerSecConfigClientAuth);
            ServerSecConfigClientAuth = nullptr;
        }
        if (ServerSecConfigDeferClientAuth) {
            CxPlatTlsSecConfigDelete(ServerSecConfigDeferClientAuth);
            ServerSecConfigDeferClientAuth = nullptr;
        }
        if (ServerSecConfig) {
            CxPlatTlsSecConfigDelete(ServerSecConfig);
            ServerSecConfig = nullptr;
        }
        if (Pkcs12SecConfig) {
            CxPlatTlsSecConfigDelete(Pkcs12SecConfig);
            Pkcs12SecConfig = nullptr;
        }
    }

    struct TlsContext
    {
        CXPLAT_TLS* Ptr;
        CXPLAT_SEC_CONFIG* SecConfig;
        CXPLAT_EVENT ProcessCompleteEvent;

        CXPLAT_TLS_PROCESS_STATE State;

        static const CXPLAT_TLS_CALLBACKS TlsServerCallbacks;
        static const CXPLAT_TLS_CALLBACKS TlsClientCallbacks;

        bool Connected;
        bool Key0RttReady;
        bool Key1RttReady;

        TlsContext() :
            Ptr(nullptr),
            SecConfig(nullptr),
            Connected(false) {
            CxPlatEventInitialize(&ProcessCompleteEvent, FALSE, FALSE);
            CxPlatZeroMemory(&State, sizeof(State));
            State.Buffer = (uint8_t*)CXPLAT_ALLOC_NONPAGED(8000, QUIC_POOL_TEST);
            State.BufferAllocLength = 8000;
        }

        ~TlsContext() {
            CxPlatTlsUninitialize(Ptr);
            CxPlatEventUninitialize(ProcessCompleteEvent);
            CXPLAT_FREE(State.Buffer, QUIC_POOL_TEST);
            for (uint8_t i = 0; i < QUIC_PACKET_KEY_COUNT; ++i) {
                QuicPacketKeyFree(State.ReadKeys[i]);
                QuicPacketKeyFree(State.WriteKeys[i]);
            }
            if (ResumptionTicket.Buffer) {
                CXPLAT_FREE(ResumptionTicket.Buffer, QUIC_POOL_CRYPTO_RESUMPTION_TICKET);
            }
        }

        void InitializeServer(
            const CXPLAT_SEC_CONFIG* SecConfiguration,
            bool MultipleAlpns = false,
            uint16_t TPLen = 64
            )
        {
            CXPLAT_TLS_CONFIG Config = {0};
            Config.IsServer = TRUE;
            Config.SecConfig = (CXPLAT_SEC_CONFIG*)SecConfiguration;
            UNREFERENCED_PARAMETER(MultipleAlpns); // The server must always send back the negotiated ALPN.
            Config.AlpnBuffer = Alpn;
            Config.AlpnBufferLength = sizeof(Alpn);
            Config.TPType = TLS_EXTENSION_TYPE_QUIC_TRANSPORT_PARAMETERS;
            Config.LocalTPBuffer =
                (uint8_t*)CXPLAT_ALLOC_NONPAGED(CxPlatTlsTPHeaderSize + TPLen, QUIC_POOL_TLS_TRANSPARAMS);
            Config.LocalTPLength = CxPlatTlsTPHeaderSize + TPLen;
            Config.Connection = (QUIC_CONNECTION*)this;
            State.NegotiatedAlpn = Alpn;

            VERIFY_QUIC_SUCCESS(
                CxPlatTlsInitialize(
                    &Config,
                    &State,
                    &Ptr));
        }

        void InitializeClient(
            CXPLAT_SEC_CONFIG* SecConfiguration,
            bool MultipleAlpns = false,
            uint16_t TPLen = 64,
            QUIC_BUFFER* Ticket = nullptr
            )
        {
            CXPLAT_TLS_CONFIG Config = {0};
            Config.IsServer = FALSE;
            Config.SecConfig = SecConfiguration;
            Config.AlpnBuffer = MultipleAlpns ? MultiAlpn : Alpn;
            Config.AlpnBufferLength = MultipleAlpns ? sizeof(MultiAlpn) : sizeof(Alpn);
            Config.TPType = TLS_EXTENSION_TYPE_QUIC_TRANSPORT_PARAMETERS;
            Config.LocalTPBuffer =
                (uint8_t*)CXPLAT_ALLOC_NONPAGED(CxPlatTlsTPHeaderSize + TPLen, QUIC_POOL_TLS_TRANSPARAMS);
            Config.LocalTPLength = CxPlatTlsTPHeaderSize + TPLen;
            Config.Connection = (QUIC_CONNECTION*)this;
            Config.ServerName = "localhost";
            if (Ticket) {
                ASSERT_NE(nullptr, Ticket->Buffer);
                //ASSERT_NE((uint32_t)0, Ticket->Length);
                Config.ResumptionTicketBuffer = Ticket->Buffer;
                Config.ResumptionTicketLength = Ticket->Length;
                Ticket->Buffer = nullptr;
            }

            VERIFY_QUIC_SUCCESS(
                CxPlatTlsInitialize(
                    &Config,
                    &State,
                    &Ptr));
        }

    private:

        static
        uint32_t
        TlsReadUint24(
            _In_reads_(3) const uint8_t* Buffer
            )
        {
            return
                (((uint32_t)Buffer[0] << 16) +
                ((uint32_t)Buffer[1] << 8) +
                (uint32_t)Buffer[2]);
        }

        static
        uint32_t
        GetCompleteTlsMessagesLength(
            _In_reads_(BufferLength)
                const uint8_t* Buffer,
            _In_ uint32_t BufferLength
            )
        {
            uint32_t MessagesLength = 0;
            do {
                if (BufferLength < 4) {
                    break;
                }
                uint32_t MessageLength = 4 + TlsReadUint24(Buffer + 1);
                if (BufferLength < MessageLength) {
                    break;
                }
                MessagesLength += MessageLength;
                Buffer += MessageLength;
                BufferLength -= MessageLength;
            } while (BufferLength > 0);
            return MessagesLength;
        }

        CXPLAT_TLS_RESULT_FLAGS
        ProcessData(
            _In_ QUIC_PACKET_KEY_TYPE BufferKey,
            _In_reads_bytes_(*BufferLength)
                const uint8_t * Buffer,
            _In_ uint32_t * BufferLength,
            _In_ bool ExpectError,
            _In_ CXPLAT_TLS_DATA_TYPE DataType
            )
        {
            CxPlatEventReset(ProcessCompleteEvent);

            EXPECT_TRUE(Buffer != nullptr || *BufferLength == 0);
            if (Buffer != nullptr) {
                EXPECT_EQ(BufferKey, State.ReadKey);
                if (DataType != CXPLAT_TLS_TICKET_DATA) {
                    *BufferLength = GetCompleteTlsMessagesLength(Buffer, *BufferLength);
                    if (*BufferLength == 0) return (CXPLAT_TLS_RESULT_FLAGS)0;
                }
            }

            //std::cout << "Processing " << *BufferLength << " bytes of type " << DataType << std::endl;

            auto Result =
                CxPlatTlsProcessData(
                    Ptr,
                    DataType,
                    Buffer,
                    BufferLength,
                    &State);
            if (Result & CXPLAT_TLS_RESULT_PENDING) {
                CxPlatEventWaitForever(ProcessCompleteEvent);
                Result = CxPlatTlsProcessDataComplete(Ptr, BufferLength);
            }

            if (!ExpectError) {
                EXPECT_TRUE((Result & CXPLAT_TLS_RESULT_ERROR) == 0);
            }

            return Result;
        }

        CXPLAT_TLS_RESULT_FLAGS
        ProcessFragmentedData(
            _In_ QUIC_PACKET_KEY_TYPE BufferKey,
            _In_reads_bytes_(BufferLength)
                const uint8_t * Buffer,
            _In_ uint32_t BufferLength,
            _In_ uint32_t FragmentSize,
            _In_ bool ExpectError,
            _In_ CXPLAT_TLS_DATA_TYPE DataType
            )
        {
            uint32_t Result = 0;
            uint32_t ConsumedBuffer = FragmentSize;
            uint32_t Count = 1;
            do {
                if (BufferLength < FragmentSize) {
                    FragmentSize = BufferLength;
                    ConsumedBuffer = FragmentSize;
                }

                //std::cout << "Processing fragment of " << FragmentSize << " bytes of type " << DataType << std::endl;

                Result |= (uint32_t)ProcessData(BufferKey, Buffer, &ConsumedBuffer, ExpectError, DataType);

                if (ConsumedBuffer > 0) {
                    Buffer += ConsumedBuffer;
                    BufferLength -= ConsumedBuffer;
                } else {
                    ConsumedBuffer = FragmentSize * ++Count;
                    ConsumedBuffer = min(ConsumedBuffer, BufferLength);
                }

            } while (BufferLength != 0 && !(Result & CXPLAT_TLS_RESULT_ERROR));

            return (CXPLAT_TLS_RESULT_FLAGS)Result;
        }

    public:

        QUIC_BUFFER ResumptionTicket {0, nullptr};

        bool OnPeerCertReceivedCalled{false};
        uint32_t ExpectedErrorFlags {0};
        QUIC_STATUS ExpectedValidationStatus {QUIC_STATUS_SUCCESS};
        BOOLEAN OnPeerCertReceivedResult{TRUE};

        CXPLAT_TLS_RESULT_FLAGS
        ProcessData(
            _Inout_ CXPLAT_TLS_PROCESS_STATE* PeerState,
            _In_ uint32_t FragmentSize = DefaultFragmentSize,
            _In_ bool ExpectError = false,
            _In_ CXPLAT_TLS_DATA_TYPE DataType = CXPLAT_TLS_CRYPTO_DATA
            )
        {
            if (PeerState == nullptr) {
                //
                // Special case for client hello/initial.
                //
                uint32_t Zero = 0;
                return ProcessData(QUIC_PACKET_KEY_INITIAL, nullptr, &Zero, ExpectError, DataType);
            }

            uint32_t Result = 0;

            do {
                uint16_t BufferLength;
                QUIC_PACKET_KEY_TYPE PeerWriteKey;

                uint32_t StartOffset = PeerState->BufferTotalLength - PeerState->BufferLength;
                if (PeerState->BufferOffset1Rtt != 0 && StartOffset >= PeerState->BufferOffset1Rtt) {
                    PeerWriteKey = QUIC_PACKET_KEY_1_RTT;
                    BufferLength = PeerState->BufferLength;

                } else if (PeerState->BufferOffsetHandshake != 0 && StartOffset >= PeerState->BufferOffsetHandshake) {
                    PeerWriteKey = QUIC_PACKET_KEY_HANDSHAKE;
                    if (PeerState->BufferOffset1Rtt != 0) {
                        BufferLength = (uint16_t)(PeerState->BufferOffset1Rtt - StartOffset);
                    } else {
                        BufferLength = PeerState->BufferLength;
                    }

                } else {
                    PeerWriteKey = QUIC_PACKET_KEY_INITIAL;
                    if (PeerState->BufferOffsetHandshake != 0) {
                        BufferLength = (uint16_t)(PeerState->BufferOffsetHandshake - StartOffset);
                    } else {
                        BufferLength = PeerState->BufferLength;
                    }
                }

                Result |=
                    (uint32_t)ProcessFragmentedData(
                        PeerWriteKey,
                        PeerState->Buffer,
                        BufferLength,
                        FragmentSize,
                        ExpectError,
                        DataType);

                PeerState->BufferLength -= BufferLength;
                CxPlatMoveMemory(
                    PeerState->Buffer,
                    PeerState->Buffer + BufferLength,
                    PeerState->BufferLength);

            } while (PeerState->BufferLength != 0 && !(Result & CXPLAT_TLS_RESULT_ERROR));

            return (CXPLAT_TLS_RESULT_FLAGS)Result;
        }

    private:

        static void
        OnProcessComplete(
            _In_ QUIC_CONNECTION* Connection
            )
        {
            CxPlatEventSet(((TlsContext*)Connection)->ProcessCompleteEvent);
        }

        static BOOLEAN
        OnRecvQuicTP(
            _In_ QUIC_CONNECTION* Connection,
            _In_ uint16_t TPLength,
            _In_reads_(TPLength) const uint8_t* TPBuffer
            )
        {
            UNREFERENCED_PARAMETER(Connection);
            UNREFERENCED_PARAMETER(TPLength);
            UNREFERENCED_PARAMETER(TPBuffer);
            return TRUE;
        }

        static BOOLEAN
        OnRecvTicketServer(
            _In_ QUIC_CONNECTION* Connection,
            _In_ uint32_t TicketLength,
            _In_reads_(TicketLength) const uint8_t* Ticket
            )
        {
            UNREFERENCED_PARAMETER(Connection);
            UNREFERENCED_PARAMETER(TicketLength);
            UNREFERENCED_PARAMETER(Ticket);
            return TRUE;
        }

        static BOOLEAN
        OnRecvTicketClient(
            _In_ QUIC_CONNECTION* Connection,
            _In_ uint32_t TicketLength,
            _In_reads_(TicketLength) const uint8_t* Ticket
            )
        {
            //std::cout << "==RecvTicketClient==" << std::endl;
            auto Context = (TlsContext*)Connection;
            if (Context->ResumptionTicket.Buffer == nullptr) {
                Context->ResumptionTicket.Buffer =
                    (uint8_t*)CXPLAT_ALLOC_NONPAGED(TicketLength, QUIC_POOL_CRYPTO_RESUMPTION_TICKET);
                Context->ResumptionTicket.Length = TicketLength;
                CxPlatCopyMemory(
                    Context->ResumptionTicket.Buffer,
                    Ticket,
                    TicketLength);
            }
            return TRUE;
        }

        static BOOLEAN
        OnPeerCertReceived(
            _In_ QUIC_CONNECTION* Connection,
            _In_ void* /* Certificate */,
            _In_ uint32_t DeferredErrorFlags,
            _In_ QUIC_STATUS DeferredStatus
            )
        {
            auto Context = (TlsContext*)Connection;
            Context->OnPeerCertReceivedCalled = true;
            if (Context->ExpectedErrorFlags != DeferredErrorFlags) {
                std::cout << "Incorrect ErrorFlags: " << DeferredErrorFlags << "\n";
                return FALSE;
            }
            if (Context->ExpectedValidationStatus != DeferredStatus) {
                std::cout << "Incorrect validation Status: " << DeferredStatus << "\n";
                return FALSE;
            }
            return Context->OnPeerCertReceivedResult;
        }
    };

    struct PacketKey
    {
        QUIC_PACKET_KEY* Ptr;
        PacketKey(QUIC_PACKET_KEY* Key) : Ptr(Key) {
            EXPECT_NE(nullptr, Key);
        }

        uint16_t
        Overhead()
        {
            return CXPLAT_ENCRYPTION_OVERHEAD;
        }

        bool
        Encrypt(
            _In_ uint16_t HeaderLength,
            _In_reads_bytes_(HeaderLength)
                const uint8_t* const Header,
            _In_ uint64_t PacketNumber,
            _In_ uint16_t BufferLength,
            _Inout_updates_bytes_(BufferLength) uint8_t* Buffer
            )
        {
            uint8_t Iv[CXPLAT_IV_LENGTH];
            QuicCryptoCombineIvAndPacketNumber(Ptr->Iv, (uint8_t*) &PacketNumber, Iv);

            return
                QUIC_STATUS_SUCCESS ==
                CxPlatEncrypt(
                    Ptr->PacketKey,
                    Iv,
                    HeaderLength,
                    Header,
                    BufferLength,
                    Buffer);
        }

        bool
        Decrypt(
            _In_ uint16_t HeaderLength,
            _In_reads_bytes_(HeaderLength)
                const uint8_t* const Header,
            _In_ uint64_t PacketNumber,
            _In_ uint16_t BufferLength,
            _Inout_updates_bytes_(BufferLength) uint8_t* Buffer
            )
        {
            uint8_t Iv[CXPLAT_IV_LENGTH];
            QuicCryptoCombineIvAndPacketNumber(Ptr->Iv, (uint8_t*) &PacketNumber, Iv);

            return
                QUIC_STATUS_SUCCESS ==
                CxPlatDecrypt(
                    Ptr->PacketKey,
                    Iv,
                    HeaderLength,
                    Header,
                    BufferLength,
                    Buffer);
        }

        bool
        ComputeHpMask(
            _In_reads_bytes_(16)
                const uint8_t* const Cipher,
            _Out_writes_bytes_(16)
                uint8_t* Mask
            )
        {
            return
                QUIC_STATUS_SUCCESS ==
                CxPlatHpComputeMask(
                    Ptr->HeaderKey,
                    1,
                    Cipher,
                    Mask);
        }
    };

    static
    void
    DoHandshake(
        TlsContext& ServerContext,
        TlsContext& ClientContext,
        uint32_t FragmentSize = DefaultFragmentSize,
        bool SendResumptionTicket = false,
        bool ServerResultError = false
        )
    {
        //std::cout << "==DoHandshake==" << std::endl;

        auto Result = ClientContext.ProcessData(nullptr);
        ASSERT_TRUE(Result & CXPLAT_TLS_RESULT_DATA);

        Result = ServerContext.ProcessData(&ClientContext.State, FragmentSize);
        ASSERT_TRUE(Result & CXPLAT_TLS_RESULT_DATA);
        ASSERT_NE(nullptr, ServerContext.State.WriteKeys[QUIC_PACKET_KEY_1_RTT]);

        Result = ClientContext.ProcessData(&ServerContext.State, FragmentSize);
        ASSERT_TRUE(Result & CXPLAT_TLS_RESULT_DATA);
        ASSERT_TRUE(Result & CXPLAT_TLS_RESULT_COMPLETE);
        ASSERT_NE(nullptr, ClientContext.State.WriteKeys[QUIC_PACKET_KEY_1_RTT]);

        Result = ServerContext.ProcessData(&ClientContext.State, FragmentSize, ServerResultError);
        if (ServerResultError) {
            ASSERT_TRUE(Result & CXPLAT_TLS_RESULT_ERROR);
        } else {
            ASSERT_TRUE(Result & CXPLAT_TLS_RESULT_COMPLETE);
        }

        if (SendResumptionTicket) {
            //std::cout << "==PostHandshake==" << std::endl;

            Result = ServerContext.ProcessData(&ClientContext.State, FragmentSize, false, CXPLAT_TLS_TICKET_DATA);
            ASSERT_TRUE(Result & CXPLAT_TLS_RESULT_DATA);

            Result = ClientContext.ProcessData(&ServerContext.State, FragmentSize);
        }
    }

    static CXPLAT_THREAD_CALLBACK(HandshakeAsync, Context)
    {
        TlsTest* This = (TlsTest*)Context;
        for (uint32_t i = 0; i < 100; ++i) {
            TlsContext ServerContext, ClientContext;
            ServerContext.InitializeServer(This->ServerSecConfig);
            ClientContext.InitializeClient(This->ClientSecConfigNoCertValidation);
            DoHandshake(ServerContext, ClientContext);
        }
        CXPLAT_THREAD_RETURN(0);
    }

    int64_t
    DoEncryption(
        PacketKey& Key,
        uint16_t BufferSize,
        uint64_t LoopCount
        )
    {
        uint8_t Header[32] = { 0 };
        uint8_t Buffer[(uint16_t)~0] = { 0 };
        uint16_t OverHead = Key.Overhead();

        uint64_t Start, End;
        Start = CxPlatTimeUs64();

        for (uint64_t j = 0; j < LoopCount; ++j) {
            Key.Encrypt(
                sizeof(Header),
                Header,
                j,
                BufferSize + OverHead,
                Buffer);
        }

        End = CxPlatTimeUs64();

        return End - Start;
    }

    int64_t
    DoEncryptionWithPNE(
        PacketKey& Key,
        uint16_t BufferSize,
        uint64_t LoopCount
        )
    {
        uint8_t Header[32] = { 0 };
        uint8_t Buffer[(uint16_t)~0] = { 0 };
        uint16_t OverHead = Key.Overhead();
        uint8_t Mask[16];

        uint64_t Start, End;
        Start = CxPlatTimeUs64();

        for (uint64_t j = 0; j < LoopCount; ++j) {
            Key.Encrypt(
                sizeof(Header),
                Header,
                j,
                BufferSize + OverHead,
                Buffer);
            Key.ComputeHpMask(Buffer, Mask);
            for (uint32_t i = 0; i < sizeof(Mask); i++) {
                Header[i] ^= Mask[i];
            }
        }

        End = CxPlatTimeUs64();

        return End - Start;
    }
};

const CXPLAT_TLS_CALLBACKS TlsTest::TlsContext::TlsServerCallbacks = {
    TlsTest::TlsContext::OnProcessComplete,
    TlsTest::TlsContext::OnRecvQuicTP,
    TlsTest::TlsContext::OnRecvTicketServer,
    TlsTest::TlsContext::OnPeerCertReceived
};

const CXPLAT_TLS_CALLBACKS TlsTest::TlsContext::TlsClientCallbacks = {
    TlsTest::TlsContext::OnProcessComplete,
    TlsTest::TlsContext::OnRecvQuicTP,
    TlsTest::TlsContext::OnRecvTicketClient,
    TlsTest::TlsContext::OnPeerCertReceived
};

QUIC_CREDENTIAL_FLAGS TlsTest::SelfSignedCertParamsFlags = QUIC_CREDENTIAL_FLAG_NONE;
QUIC_CREDENTIAL_CONFIG* TlsTest::SelfSignedCertParams = nullptr;
QUIC_CREDENTIAL_CONFIG* TlsTest::ClientCertParams = nullptr;
QUIC_CREDENTIAL_CONFIG* TlsTest::CertParamsFromFile = nullptr;

TEST_F(TlsTest, Initialize)
{
    TlsContext ServerContext, ClientContext;
    ServerContext.InitializeServer(ServerSecConfig);
    ClientContext.InitializeClient(ClientSecConfigNoCertValidation);
}

TEST_F(TlsTest, Handshake)
{
    TlsContext ServerContext, ClientContext;
    ServerContext.InitializeServer(ServerSecConfig);
    ClientContext.InitializeClient(ClientSecConfigNoCertValidation);
    DoHandshake(ServerContext, ClientContext);
}

TEST_F(TlsTest, HandshakeParamInfoAES256GCM)
{
    TlsContext ServerContext, ClientContext;
    ServerContext.InitializeServer(ServerSecConfig);
    ClientContext.InitializeClient(ClientSecConfigNoCertValidation);
    DoHandshake(ServerContext, ClientContext);

    QUIC_HANDSHAKE_INFO HandshakeInfo;
    CxPlatZeroMemory(&HandshakeInfo, sizeof(HandshakeInfo));
    uint32_t HandshakeInfoLen = sizeof(HandshakeInfo);
    QUIC_STATUS Status =
        CxPlatTlsParamGet(
            ClientContext.Ptr,
            QUIC_PARAM_TLS_HANDSHAKE_INFO,
            &HandshakeInfoLen,
            &HandshakeInfo);
    ASSERT_TRUE(QUIC_SUCCEEDED(Status));
    EXPECT_EQ(QUIC_CIPHER_SUITE_TLS_AES_256_GCM_SHA384, HandshakeInfo.CipherSuite);
    EXPECT_EQ(QUIC_TLS_PROTOCOL_1_3, HandshakeInfo.TlsProtocolVersion);
    EXPECT_EQ(QUIC_CIPHER_ALGORITHM_AES_256, HandshakeInfo.CipherAlgorithm);
    EXPECT_EQ(256, HandshakeInfo.CipherStrength);
    EXPECT_EQ(0, HandshakeInfo.KeyExchangeAlgorithm);
    EXPECT_EQ(0, HandshakeInfo.KeyExchangeStrength);
    EXPECT_EQ(QUIC_HASH_ALGORITHM_SHA_384, HandshakeInfo.Hash);
    EXPECT_EQ(0, HandshakeInfo.HashStrength);

    CxPlatZeroMemory(&HandshakeInfo, sizeof(HandshakeInfo));
    HandshakeInfoLen = sizeof(HandshakeInfo);
    Status =
        CxPlatTlsParamGet(
            ServerContext.Ptr,
            QUIC_PARAM_TLS_HANDSHAKE_INFO,
            &HandshakeInfoLen,
            &HandshakeInfo);
    ASSERT_TRUE(QUIC_SUCCEEDED(Status));
    EXPECT_EQ(QUIC_CIPHER_SUITE_TLS_AES_256_GCM_SHA384, HandshakeInfo.CipherSuite);
    EXPECT_EQ(QUIC_TLS_PROTOCOL_1_3, HandshakeInfo.TlsProtocolVersion);
    EXPECT_EQ(QUIC_CIPHER_ALGORITHM_AES_256, HandshakeInfo.CipherAlgorithm);
    EXPECT_EQ(256, HandshakeInfo.CipherStrength);
    EXPECT_EQ(0, HandshakeInfo.KeyExchangeAlgorithm);
    EXPECT_EQ(0, HandshakeInfo.KeyExchangeStrength);
    EXPECT_EQ(QUIC_HASH_ALGORITHM_SHA_384, HandshakeInfo.Hash);
    EXPECT_EQ(0, HandshakeInfo.HashStrength);
}

// Disabled until we have a way to switch ciphers
// TEST_F(TlsTest, HandshakeParamInfoAES128GCM)
// {
//     TlsContext ServerContext, ClientContext;
//     ServerContext.InitializeServer(ServerSecConfig);
//     ClientContext.InitializeClient(ClientSecConfigNoCertValidation);
//     DoHandshake(ServerContext, ClientContext);

//     QUIC_HANDSHAKE_INFO HandshakeInfo;
//     CxPlatZeroMemory(&HandshakeInfo, sizeof(HandshakeInfo));
//     uint32_t HandshakeInfoLen = sizeof(HandshakeInfo);
//     QUIC_STATUS Status =
//         CxPlatTlsParamGet(
//             ClientContext.Ptr,
//             QUIC_PARAM_TLS_HANDSHAKE_INFO,
//             &HandshakeInfoLen,
//             &HandshakeInfo);
//     ASSERT_TRUE(QUIC_SUCCEEDED(Status));
//     EXPECT_EQ(QUIC_CIPHER_SUITE_TLS_AES_128_GCM_SHA256, HandshakeInfo.CipherSuite);
//     EXPECT_EQ(QUIC_TLS1_3_CLIENT, HandshakeInfo.TlsProtocolVersion);
//     EXPECT_EQ(QUIC_ALG_AES_128, HandshakeInfo.CipherAlgorithm);
//     EXPECT_EQ(128, HandshakeInfo.CipherStrength);
//     EXPECT_EQ(0, HandshakeInfo.KeyExchangeAlgorithm);
//     EXPECT_EQ(0, HandshakeInfo.KeyExchangeStrength);
//     EXPECT_EQ(QUIC_ALG_SHA_256, HandshakeInfo.Hash);
//     EXPECT_EQ(0, HandshakeInfo.HashStrength);

//     CxPlatZeroMemory(&HandshakeInfo, sizeof(HandshakeInfo));
//     HandshakeInfoLen = sizeof(HandshakeInfo);
//     Status =
//         CxPlatTlsParamGet(
//             ServerContext.Ptr,
//             QUIC_PARAM_TLS_HANDSHAKE_INFO,
//             &HandshakeInfoLen,
//             &HandshakeInfo);
//     ASSERT_TRUE(QUIC_SUCCEEDED(Status));
//     EXPECT_EQ(QUIC_CIPHER_SUITE_TLS_AES_128_GCM_SHA256, HandshakeInfo.CipherSuite);
//     EXPECT_EQ(QUIC_TLS1_3_SERVER, HandshakeInfo.TlsProtocolVersion);
//     EXPECT_EQ(QUIC_ALG_AES_128, HandshakeInfo.CipherAlgorithm);
//     EXPECT_EQ(128, HandshakeInfo.CipherStrength);
//     EXPECT_EQ(0, HandshakeInfo.KeyExchangeAlgorithm);
//     EXPECT_EQ(0, HandshakeInfo.KeyExchangeStrength);
//     EXPECT_EQ(QUIC_ALG_SHA_256, HandshakeInfo.Hash);
//     EXPECT_EQ(0, HandshakeInfo.HashStrength);
// }

TEST_F(TlsTest, HandshakeParamNegotiatedAlpn)
{
    TlsContext ServerContext, ClientContext;
    ServerContext.InitializeServer(ServerSecConfig);
    ClientContext.InitializeClient(ClientSecConfigNoCertValidation);
    DoHandshake(ServerContext, ClientContext);

    char NegotiatedAlpn[255];
    CxPlatZeroMemory(&NegotiatedAlpn, sizeof(NegotiatedAlpn));
    uint32_t AlpnLen = sizeof(NegotiatedAlpn);
    QUIC_STATUS Status =
        CxPlatTlsParamGet(
            ClientContext.Ptr,
            QUIC_PARAM_TLS_NEGOTIATED_ALPN,
            &AlpnLen,
            NegotiatedAlpn);
    ASSERT_TRUE(QUIC_SUCCEEDED(Status));
    ASSERT_EQ(Alpn[0], AlpnLen);
    ASSERT_EQ(Alpn[1], NegotiatedAlpn[0]);

    CxPlatZeroMemory(&NegotiatedAlpn, sizeof(NegotiatedAlpn));
    AlpnLen = sizeof(NegotiatedAlpn);
    Status =
        CxPlatTlsParamGet(
            ServerContext.Ptr,
            QUIC_PARAM_TLS_NEGOTIATED_ALPN,
            &AlpnLen,
            NegotiatedAlpn);
    ASSERT_TRUE(QUIC_SUCCEEDED(Status));
    ASSERT_EQ(Alpn[0], AlpnLen);
    ASSERT_EQ(Alpn[1], NegotiatedAlpn[0]);
}

TEST_F(TlsTest, HandshakeParallel)
{
    CXPLAT_THREAD_CONFIG Config = {
        0,
        0,
        "TlsWorker",
        HandshakeAsync,
        this
    };

    CXPLAT_THREAD Threads[64];
    CxPlatZeroMemory(&Threads, sizeof(Threads));

    for (uint32_t i = 0; i < ARRAYSIZE(Threads); ++i) {
        VERIFY_QUIC_SUCCESS(CxPlatThreadCreate(&Config, &Threads[i]));
    }

    for (uint32_t i = 0; i < ARRAYSIZE(Threads); ++i) {
        CxPlatThreadWait(&Threads[i]);
        CxPlatThreadDelete(&Threads[i]);
    }
}

/*#ifndef QUIC_DISABLE_0RTT_TESTS
TEST_F(TlsTest, HandshakeResumption)
{
    TlsContext ServerContext, ClientContext;
    ServerContext.InitializeServer(ServerSecConfig);
    ClientContext.InitializeClient(ClientSecConfigNoCertValidation);
    DoHandshake(ServerContext, ClientContext, DefaultFragmentSize, true);

    ASSERT_NE(nullptr, ClientContext.ResumptionTicket.Buffer);
    //ASSERT_NE((uint32_t)0, ClientContext.ResumptionTicket.Length);

    TlsContext ServerContext2, ClientContext2;
    ServerContext2.InitializeServer(ServerSecConfig);
    ClientContext2.InitializeClient(ClientSecConfigNoCertValidation, false, 64, &ClientContext.ResumptionTicket);
    DoHandshake(ServerContext2, ClientContext2);
}
#endif*/

TEST_F(TlsTest, HandshakeMultiAlpnServer)
{
    TlsContext ServerContext, ClientContext;
    ServerContext.InitializeServer(ServerSecConfig, true);
    ClientContext.InitializeClient(ClientSecConfigNoCertValidation);
    DoHandshake(ServerContext, ClientContext);
}

TEST_F(TlsTest, HandshakeMultiAlpnClient)
{
    TlsContext ServerContext, ClientContext;
    ServerContext.InitializeServer(ServerSecConfig);
    ClientContext.InitializeClient(ClientSecConfigNoCertValidation, true);
    DoHandshake(ServerContext, ClientContext);
}

TEST_F(TlsTest, HandshakeMultiAlpnBoth)
{
    TlsContext ServerContext, ClientContext;
    ServerContext.InitializeServer(ServerSecConfig, true);
    ClientContext.InitializeClient(ClientSecConfigNoCertValidation, true);
    DoHandshake(ServerContext, ClientContext);
}

TEST_F(TlsTest, HandshakeFragmented)
{
    TlsContext ServerContext, ClientContext;
    ServerContext.InitializeServer(ServerSecConfig);
    ClientContext.InitializeClient(ClientSecConfigNoCertValidation);
    DoHandshake(ServerContext, ClientContext, 200);
}

TEST_F(TlsTest, HandshakeVeryFragmented)
{
    TlsContext ServerContext, ClientContext;
    ServerContext.InitializeServer(ServerSecConfig, false, 1500);
    ClientContext.InitializeClient(ClientSecConfigNoCertValidation, false, 1500);
    DoHandshake(ServerContext, ClientContext, 1);
}

TEST_F(TlsTest, HandshakesSerial)
{
    {
        TlsContext ServerContext, ClientContext1;
        ServerContext.InitializeServer(ServerSecConfig);
        ClientContext1.InitializeClient(ClientSecConfigNoCertValidation);
        DoHandshake(ServerContext, ClientContext1);
    }
    {
        TlsContext ServerContext, ClientContext2;
        ServerContext.InitializeServer(ServerSecConfig);
        ClientContext2.InitializeClient(ClientSecConfigNoCertValidation);
        DoHandshake(ServerContext, ClientContext2);
    }
}

TEST_F(TlsTest, HandshakesInterleaved)
{
    TlsContext ServerContext1, ServerContext2, ClientContext1, ClientContext2;
    ServerContext1.InitializeServer(ServerSecConfig);
    ClientContext1.InitializeClient(ClientSecConfigNoCertValidation);
    ServerContext2.InitializeServer(ServerSecConfig);
    ClientContext2.InitializeClient(ClientSecConfigNoCertValidation);

    auto Result = ClientContext1.ProcessData(nullptr);
    ASSERT_TRUE(Result & CXPLAT_TLS_RESULT_DATA);

    Result = ClientContext2.ProcessData(nullptr);
    ASSERT_TRUE(Result & CXPLAT_TLS_RESULT_DATA);

    Result = ServerContext1.ProcessData(&ClientContext1.State);
    ASSERT_TRUE(Result & CXPLAT_TLS_RESULT_DATA);
    ASSERT_NE(nullptr, ServerContext1.State.WriteKeys[QUIC_PACKET_KEY_1_RTT]);

    Result = ServerContext2.ProcessData(&ClientContext2.State);
    ASSERT_TRUE(Result & CXPLAT_TLS_RESULT_DATA);
    ASSERT_NE(nullptr, ServerContext2.State.WriteKeys[QUIC_PACKET_KEY_1_RTT]);

    Result = ClientContext1.ProcessData(&ServerContext1.State);
    ASSERT_TRUE(Result & CXPLAT_TLS_RESULT_DATA);
    ASSERT_TRUE(Result & CXPLAT_TLS_RESULT_COMPLETE);
    ASSERT_NE(nullptr, ClientContext1.State.WriteKeys[QUIC_PACKET_KEY_1_RTT]);

    Result = ClientContext2.ProcessData(&ServerContext2.State);
    ASSERT_TRUE(Result & CXPLAT_TLS_RESULT_DATA);
    ASSERT_TRUE(Result & CXPLAT_TLS_RESULT_COMPLETE);
    ASSERT_NE(nullptr, ClientContext2.State.WriteKeys[QUIC_PACKET_KEY_1_RTT]);

    Result = ServerContext1.ProcessData(&ClientContext1.State);
    ASSERT_TRUE(Result & CXPLAT_TLS_RESULT_COMPLETE);

    Result = ServerContext2.ProcessData(&ClientContext2.State);
    ASSERT_TRUE(Result & CXPLAT_TLS_RESULT_COMPLETE);
}

TEST_F(TlsTest, CertificateError)
{
    TlsContext ServerContext, ClientContext;
    ServerContext.InitializeServer(ServerSecConfig);
    ClientContext.InitializeClient(ClientSecConfig);
    {
        auto Result = ClientContext.ProcessData(nullptr);
        ASSERT_TRUE(Result & CXPLAT_TLS_RESULT_DATA);

        Result = ServerContext.ProcessData(&ClientContext.State);
        ASSERT_TRUE(Result & CXPLAT_TLS_RESULT_DATA);
        ASSERT_NE(nullptr, ServerContext.State.WriteKeys[QUIC_PACKET_KEY_1_RTT]);

        Result = ClientContext.ProcessData(&ServerContext.State, DefaultFragmentSize, true);
        ASSERT_TRUE(Result & CXPLAT_TLS_RESULT_ERROR);
        ASSERT_TRUE(
            (0xFF & ClientContext.State.AlertCode) == CXPLAT_TLS_ALERT_CODE_BAD_CERTIFICATE ||
            (0xFF & ClientContext.State.AlertCode) == CXPLAT_TLS_ALERT_CODE_UNKNOWN_CA);
    }
}

TEST_F(TlsTest, DeferredCertificateValidationAllow)
{
    if (!ClientSecConfigDeferredCertValidation) {
        std::cout << "WARNING: Test unsupported\n";
        return; // Unsupported by platform
    }

    TlsContext ServerContext, ClientContext;
    ServerContext.InitializeServer(ServerSecConfig);
    ClientContext.InitializeClient(ClientSecConfigDeferredCertValidation);
    ClientContext.ExpectedValidationStatus = QUIC_STATUS_CERT_UNTRUSTED_ROOT;
#ifdef _WIN32
    ClientContext.ExpectedErrorFlags = CERT_TRUST_IS_UNTRUSTED_ROOT;
#else
    // TODO - Add platform specific values if support is added.
#endif
    {
        auto Result = ClientContext.ProcessData(nullptr);
        ASSERT_TRUE(Result & CXPLAT_TLS_RESULT_DATA);

        Result = ServerContext.ProcessData(&ClientContext.State);
        ASSERT_TRUE(Result & CXPLAT_TLS_RESULT_DATA);
        ASSERT_NE(nullptr, ServerContext.State.WriteKeys[QUIC_PACKET_KEY_1_RTT]);

        Result = ClientContext.ProcessData(&ServerContext.State, DefaultFragmentSize, true);
        ASSERT_TRUE(ClientContext.OnPeerCertReceivedCalled);
        ASSERT_TRUE(Result & CXPLAT_TLS_RESULT_COMPLETE);
    }
}

TEST_F(TlsTest, DeferredCertificateValidationReject)
{
    if (!ClientSecConfigDeferredCertValidation) {
        std::cout << "WARNING: Test unsupported\n";
        return; // Unsupported by platform
    }

    TlsContext ServerContext, ClientContext;
    ServerContext.InitializeServer(ServerSecConfig);
    ClientContext.InitializeClient(ClientSecConfigDeferredCertValidation);
    {
        auto Result = ClientContext.ProcessData(nullptr);
        ASSERT_TRUE(Result & CXPLAT_TLS_RESULT_DATA);

        Result = ServerContext.ProcessData(&ClientContext.State);
        ASSERT_TRUE(Result & CXPLAT_TLS_RESULT_DATA);
        ASSERT_NE(nullptr, ServerContext.State.WriteKeys[QUIC_PACKET_KEY_1_RTT]);

        Result = ClientContext.ProcessData(&ServerContext.State, DefaultFragmentSize, true);
        ASSERT_TRUE(ClientContext.OnPeerCertReceivedCalled);
        ASSERT_TRUE(Result & CXPLAT_TLS_RESULT_ERROR);
        ASSERT_EQ((0xFF & ClientContext.State.AlertCode), CXPLAT_TLS_ALERT_CODE_BAD_CERTIFICATE);
    }
}

TEST_F(TlsTest, CustomCertificateValidationAllow)
{
    TlsContext ServerContext, ClientContext;
    ServerContext.InitializeServer(ServerSecConfig);
    ClientContext.InitializeClient(ClientSecConfigCustomCertValidation);
    {
        auto Result = ClientContext.ProcessData(nullptr);
        ASSERT_TRUE(Result & CXPLAT_TLS_RESULT_DATA);

        Result = ServerContext.ProcessData(&ClientContext.State);
        ASSERT_TRUE(Result & CXPLAT_TLS_RESULT_DATA);
        ASSERT_NE(nullptr, ServerContext.State.WriteKeys[QUIC_PACKET_KEY_1_RTT]);

        Result = ClientContext.ProcessData(&ServerContext.State, DefaultFragmentSize, true);
        ASSERT_TRUE(ClientContext.OnPeerCertReceivedCalled);
        ASSERT_TRUE(Result & CXPLAT_TLS_RESULT_COMPLETE);
    }
}

TEST_F(TlsTest, CustomCertificateValidationReject)
{
    TlsContext ServerContext, ClientContext;
    ServerContext.InitializeServer(ServerSecConfig);
    ClientContext.InitializeClient(ClientSecConfigCustomCertValidation);
    ClientContext.OnPeerCertReceivedResult = FALSE;
    {
        auto Result = ClientContext.ProcessData(nullptr);
        ASSERT_TRUE(Result & CXPLAT_TLS_RESULT_DATA);

        Result = ServerContext.ProcessData(&ClientContext.State);
        ASSERT_TRUE(Result & CXPLAT_TLS_RESULT_DATA);
        ASSERT_NE(nullptr, ServerContext.State.WriteKeys[QUIC_PACKET_KEY_1_RTT]);

        Result = ClientContext.ProcessData(&ServerContext.State, DefaultFragmentSize, true);
        ASSERT_TRUE(ClientContext.OnPeerCertReceivedCalled);
        ASSERT_TRUE(Result & CXPLAT_TLS_RESULT_ERROR);
        ASSERT_EQ((0xFF & ClientContext.State.AlertCode), CXPLAT_TLS_ALERT_CODE_BAD_CERTIFICATE);
    }
}

TEST_F(TlsTest, ExtraCertificateValidation)
{
    TlsContext ServerContext, ClientContext;
    ServerContext.InitializeServer(ServerSecConfig);
    ClientContext.InitializeClient(ClientSecConfigExtraCertValidation);
    {
        auto Result = ClientContext.ProcessData(nullptr);
        ASSERT_TRUE(Result & CXPLAT_TLS_RESULT_DATA);

        Result = ServerContext.ProcessData(&ClientContext.State);
        ASSERT_TRUE(Result & CXPLAT_TLS_RESULT_DATA);
        ASSERT_NE(nullptr, ServerContext.State.WriteKeys[QUIC_PACKET_KEY_1_RTT]);

        Result = ClientContext.ProcessData(&ServerContext.State, DefaultFragmentSize, true);
        ASSERT_FALSE(ClientContext.OnPeerCertReceivedCalled);
        ASSERT_TRUE(Result & CXPLAT_TLS_RESULT_ERROR);
        ASSERT_TRUE(
            (0xFF & ClientContext.State.AlertCode) == CXPLAT_TLS_ALERT_CODE_BAD_CERTIFICATE ||
            (0xFF & ClientContext.State.AlertCode) == CXPLAT_TLS_ALERT_CODE_UNKNOWN_CA);
    }
}

TEST_P(TlsTest, One1RttKey)
{
    bool PNE = GetParam();

    TlsContext ServerContext, ClientContext;
    ServerContext.InitializeServer(ServerSecConfig);
    ClientContext.InitializeClient(ClientSecConfigNoCertValidation);
    DoHandshake(ServerContext, ClientContext);

    PacketKey ServerKey(ServerContext.State.WriteKeys[QUIC_PACKET_KEY_1_RTT]);
    PacketKey ClientKey(ClientContext.State.ReadKeys[QUIC_PACKET_KEY_1_RTT]);

    uint8_t Header[32] = { 1, 2, 3, 4 };
    uint64_t PacketNumber = 0;
    uint8_t Buffer[1000] = { 0 };

    ASSERT_TRUE(
        ServerKey.Encrypt(
            sizeof(Header),
            Header,
            PacketNumber,
            sizeof(Buffer),
            Buffer));

    if (PNE) {
        uint8_t Mask[16];

        ASSERT_TRUE(
            ServerKey.ComputeHpMask(
                Buffer,
                Mask));

        for (uint32_t i = 0; i < sizeof(Mask); i++) {
            Header[i] ^= Mask[i];
        }

        ASSERT_TRUE(
            ClientKey.ComputeHpMask(
                Buffer,
                Mask));

        for (uint32_t i = 0; i < sizeof(Mask); i++) {
            Header[i] ^= Mask[i];
        }
    }

    ASSERT_TRUE(
        ClientKey.Decrypt(
            sizeof(Header),
            Header,
            PacketNumber,
            sizeof(Buffer),
            Buffer));
}

TEST_P(TlsTest, KeyUpdate)
{
    bool PNE = GetParam();

    TlsContext ServerContext, ClientContext;
    ServerContext.InitializeServer(ServerSecConfig);
    ClientContext.InitializeClient(ClientSecConfigNoCertValidation);
    DoHandshake(ServerContext, ClientContext);

    QUIC_PACKET_KEY* UpdateWriteKey = nullptr, *UpdateReadKey = nullptr;

    VERIFY_QUIC_SUCCESS(
        QuicPacketKeyUpdate(
            ServerContext.State.WriteKeys[QUIC_PACKET_KEY_1_RTT],
            &UpdateWriteKey));
    VERIFY_QUIC_SUCCESS(
        QuicPacketKeyUpdate(
            ClientContext.State.ReadKeys[QUIC_PACKET_KEY_1_RTT],
            &UpdateReadKey));

    if (PNE) {
        //
        // If PNE is enabled, copy the header keys to the new packet
        // key structs.
        //
        UpdateWriteKey->HeaderKey = ServerContext.State.WriteKeys[QUIC_PACKET_KEY_1_RTT]->HeaderKey;
        ServerContext.State.WriteKeys[QUIC_PACKET_KEY_1_RTT]->HeaderKey = NULL;

        UpdateReadKey->HeaderKey = ClientContext.State.ReadKeys[QUIC_PACKET_KEY_1_RTT]->HeaderKey;
        ClientContext.State.ReadKeys[QUIC_PACKET_KEY_1_RTT]->HeaderKey = NULL;
    }

    PacketKey ServerKey(UpdateWriteKey);
    PacketKey ClientKey(UpdateReadKey);

    uint8_t Header[32] = { 1, 2, 3, 4 };
    uint64_t PacketNumber = 0;
    uint8_t Buffer[1000] = { 0 };

    ASSERT_TRUE(
        ServerKey.Encrypt(
            sizeof(Header),
            Header,
            PacketNumber,
            sizeof(Buffer),
            Buffer));

    if (PNE) {
        uint8_t Mask[16];

        ASSERT_TRUE(
            ServerKey.ComputeHpMask(
                Buffer,
                Mask));

        for (uint32_t i = 0; i < sizeof(Mask); i++) {
            Header[i] ^= Mask[i];
        }

        ASSERT_TRUE(
            ClientKey.ComputeHpMask(
                Buffer,
                Mask));

        for (uint32_t i = 0; i < sizeof(Mask); i++) {
            Header[i] ^= Mask[i];
        }
    }

    ASSERT_TRUE(
        ClientKey.Decrypt(
            sizeof(Header),
            Header,
            PacketNumber,
            sizeof(Buffer),
            Buffer));

    QuicPacketKeyFree(UpdateWriteKey);
    QuicPacketKeyFree(UpdateReadKey);
}


TEST_P(TlsTest, PacketEncryptionPerf)
{
    bool PNE = GetParam();

    TlsContext ServerContext, ClientContext;
    ServerContext.InitializeServer(ServerSecConfig);
    ClientContext.InitializeClient(ClientSecConfigNoCertValidation);
    DoHandshake(ServerContext, ClientContext);

    PacketKey ServerKey(ServerContext.State.WriteKeys[QUIC_PACKET_KEY_1_RTT]);

    const uint64_t LoopCount = 10000;
    uint16_t BufferSizes[] =
    {
        4,
        16,
        64,
        256,
        600,
        1000,
        1200,
        1450,
        //8000,
        //65000
    };

#ifdef _WIN32
    HANDLE CurrentThread = GetCurrentThread();
    DWORD ProcNumber = GetCurrentProcessorNumber();
    DWORD_PTR OldAffinityMask =
        SetThreadAffinityMask(CurrentThread, (DWORD_PTR)1 << (DWORD_PTR)ProcNumber);
    SetThreadPriority(CurrentThread, THREAD_PRIORITY_HIGHEST);
#endif

    for (uint8_t i = 0; i < ARRAYSIZE(BufferSizes); ++i) {
        int64_t elapsedMicroseconds =
            PNE == 0 ?
            DoEncryption(ServerKey, BufferSizes[i], LoopCount) :
            DoEncryptionWithPNE(ServerKey, BufferSizes[i], LoopCount);

        std::cout << elapsedMicroseconds / 1000 << "." << (int)(elapsedMicroseconds % 1000) <<
            " milliseconds elapsed encrypting "
            << BufferSizes[i] << " bytes " << LoopCount << " times" << std::endl;
    }

#ifdef _WIN32
    SetThreadPriority(CurrentThread, THREAD_PRIORITY_NORMAL);
    SetThreadAffinityMask(CurrentThread, OldAffinityMask);
#endif
}

uint64_t LockedCounter(
    const uint64_t LoopCount
    )
{
    uint64_t Start, End;
    CXPLAT_DISPATCH_LOCK Lock;
    uint64_t Counter = 0;

    CxPlatDispatchLockInitialize(&Lock);
    Start = CxPlatTimeUs64();
    for (uint64_t j = 0; j < LoopCount; ++j) {
        CxPlatDispatchLockAcquire(&Lock);
        Counter++;
        CxPlatDispatchLockRelease(&Lock);
    }
    End = CxPlatTimeUs64();

    CxPlatDispatchLockUninitialize(&Lock);

    CXPLAT_FRE_ASSERT(Counter == LoopCount);

    return End - Start;
}

uint64_t InterlockedCounter(
    const uint64_t LoopCount
    )
{
    uint64_t Start, End;
    int64_t Counter = 0;

    Start = CxPlatTimeUs64();
    for (uint64_t j = 0; j < LoopCount; ++j) {
        InterlockedIncrement64(&Counter);
    }
    End = CxPlatTimeUs64();

    CXPLAT_FRE_ASSERT((uint64_t)Counter == LoopCount);

    return End - Start;
}

uint64_t UnlockedCounter(
    const uint64_t LoopCount
    )
{
    uint64_t Start, End;
    uint64_t Counter = 0;
    Start = CxPlatTimeUs64();
    for (uint64_t j = 0; j < LoopCount; ++j) {
        Counter++;
    }
    End = CxPlatTimeUs64();

    CXPLAT_FRE_ASSERT(Counter == LoopCount);

    return End - Start;
}


TEST_F(TlsTest, LockPerfTest)
{
    uint64_t (*const TestFuncs[]) (uint64_t) = {LockedCounter, InterlockedCounter, UnlockedCounter};
    const char* const TestName[] = {"Locking/unlocking", "Interlocked incrementing", "Unlocked incrementing"};
    const uint64_t LoopCount = 100000;

#ifdef _WIN32
    HANDLE CurrentThread = GetCurrentThread();
    DWORD ProcNumber = GetCurrentProcessorNumber();
    DWORD_PTR OldAffinityMask =
        SetThreadAffinityMask(CurrentThread, (DWORD_PTR)1 << (DWORD_PTR)ProcNumber);
    SetThreadPriority(CurrentThread, THREAD_PRIORITY_HIGHEST);
#endif

    for (uint8_t i = 0; i < ARRAYSIZE(TestName); ++i) {

        const uint64_t elapsedMicroseconds = TestFuncs[i](LoopCount);

        std::cout << elapsedMicroseconds / 1000 << "." << (int)(elapsedMicroseconds % 1000) <<
            " milliseconds elapsed "
            << TestName[i] << " counter " << LoopCount << " times" << std::endl;
    }

#ifdef _WIN32
    SetThreadPriority(CurrentThread, THREAD_PRIORITY_NORMAL);
    SetThreadAffinityMask(CurrentThread, OldAffinityMask);
#endif
}

#ifndef QUIC_DISABLE_CLIENT_CERT_TESTS
TEST_F(TlsTest, ClientCertificateFailValidation)
{
    TlsContext ServerContext, ClientContext;
    ServerContext.InitializeServer(ServerSecConfigClientAuth);
    ClientContext.InitializeClient(ClientSecConfigClientCertNoCertValidation);

    DoHandshake(ServerContext, ClientContext, DefaultFragmentSize, false, true);
}

TEST_F(TlsTest, ClientCertificateDeferValidation)
{
    TlsContext ServerContext, ClientContext;
    ServerContext.InitializeServer(ServerSecConfigDeferClientAuth);
    ServerContext.ExpectedValidationStatus = QUIC_STATUS_CERT_UNTRUSTED_ROOT;
    ClientContext.InitializeClient(ClientSecConfigClientCertNoCertValidation);

    DoHandshake(ServerContext, ClientContext);
}
#endif

INSTANTIATE_TEST_SUITE_P(TlsTest, TlsTest, ::testing::Bool());
