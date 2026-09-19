#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <atomic>
#include <string>

namespace cadence::capture {
// Local named pipe: synchronous inherited read end for FFmpeg, overlapped
// non-inherited write end so an unresponsive encoder cannot block forever.
class BoundedPipeWriter {
    HANDLE pipe_{INVALID_HANDLE_VALUE},event_{};
    DWORD error_{};
public:
    BoundedPipeWriter()=default;
    BoundedPipeWriter(const BoundedPipeWriter&)=delete;
    BoundedPipeWriter& operator=(const BoundedPipeWriter&)=delete;
    ~BoundedPipeWriter(){close();}
    void close(){if(pipe_!=INVALID_HANDLE_VALUE){CloseHandle(pipe_);pipe_=INVALID_HANDLE_VALUE;}if(event_){CloseHandle(event_);event_=nullptr;}}
    DWORD error()const{return error_;}
    bool open(HANDLE& reader){
        close();reader=nullptr;error_=0;static std::atomic<unsigned long long> serial{};
        const auto name=L"\\\\.\\pipe\\CadenceViewport_"+std::to_wstring(GetCurrentProcessId())+L"_"+std::to_wstring(++serial);
        pipe_=CreateNamedPipeW(name.c_str(),PIPE_ACCESS_OUTBOUND|FILE_FLAG_OVERLAPPED|FILE_FLAG_FIRST_PIPE_INSTANCE,
            PIPE_TYPE_BYTE|PIPE_READMODE_BYTE|PIPE_WAIT|PIPE_REJECT_REMOTE_CLIENTS,1,65536,65536,0,nullptr);
        if(pipe_==INVALID_HANDLE_VALUE){error_=GetLastError();return false;}
        event_=CreateEventW(nullptr,TRUE,FALSE,nullptr);
        if(!event_){error_=GetLastError();close();return false;}
        SECURITY_ATTRIBUTES attributes{sizeof(attributes),nullptr,TRUE};
        reader=CreateFileW(name.c_str(),GENERIC_READ,0,&attributes,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
        if(reader==INVALID_HANDLE_VALUE){reader=nullptr;error_=GetLastError();close();return false;}
        OVERLAPPED connection{};connection.hEvent=event_;
        if(!ConnectNamedPipe(pipe_,&connection)&&GetLastError()!=ERROR_PIPE_CONNECTED){
            error_=GetLastError();CloseHandle(reader);reader=nullptr;close();return false;
        }
        return true;
    }
    bool write(const void* data,DWORD size,DWORD& written,DWORD timeoutMs=5000){
        error_=0;written=0;ResetEvent(event_);OVERLAPPED operation{};operation.hEvent=event_;
        if(WriteFile(pipe_,data,size,&written,&operation))return written!=0;
        error_=GetLastError();if(error_!=ERROR_IO_PENDING)return false;
        const auto wait=WaitForSingleObject(event_,timeoutMs);
        if(wait!=WAIT_OBJECT_0){
            const auto failure=wait==WAIT_TIMEOUT?ERROR_TIMEOUT:GetLastError();
            CancelIoEx(pipe_,&operation);
            // Retire the canceled local-pipe operation before its OVERLAPPED
            // storage or caller's frame buffer can be released/reused.
            GetOverlappedResult(pipe_,&operation,&written,TRUE);
            error_=failure;return false;
        }
        if(!GetOverlappedResult(pipe_,&operation,&written,FALSE)){error_=GetLastError();return false;}
        error_=0;return written!=0;
    }
};
}
