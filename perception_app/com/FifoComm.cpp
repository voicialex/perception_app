#include "FifoComm.hpp"
#include "Logger.hpp"
#include <chrono>
#include <cstring>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

// FifoCommImpl implementation
FifoCommImpl::FifoCommImpl(const std::string& basePath, CommRole role)
    : basePath_(basePath), inPipePath_(basePath_ + "_in"), outPipePath_(basePath_ + "_out"),
      isServer_(false), role_(role), readFd_(-1), writeFd_(-1) {
    // Pipe path setup completed in initialization list
}

FifoCommImpl::~FifoCommImpl() {
    cleanup();
}

bool FifoCommImpl::initialize() {
    return initialize(role_);
}

bool FifoCommImpl::initialize(CommRole role) {
    if (role == CommRole::SERVER) {
        if (initializeAsServer()) {
            isServer_ = true;
            LOG_INFO("FIFO communication initialized successfully (server mode)");
            return true;
        }
        LOG_ERROR("FIFO communication initialization failed, cannot initialize as server");
        return false;
    } else if (role == CommRole::CLIENT) {
        if (initializeAsClient()) {
            isServer_ = false;
            LOG_INFO("FIFO communication initialized successfully (client mode)");
            return true;
        }
        LOG_ERROR("FIFO communication initialization failed, cannot initialize as client");
        return false;
    } else {
        // AUTO is compatible with existing logic
        if (initializeAsServer()) {
            isServer_ = true;
            LOG_INFO("FIFO communication initialized successfully (server mode)");
            return true;
        }
        if (initializeAsClient()) {
            isServer_ = false;
            LOG_INFO("FIFO communication initialized successfully (client mode)");
            return true;
        }
        LOG_ERROR("FIFO communication initialization failed, cannot initialize as server or client");
        return false;
    }
}

bool FifoCommImpl::initializeAsServer() {
    LOG_INFO("Attempting to initialize as server...");
    
    // Set server flag
    isServer_ = true;
    
    // Create pipes
    if (!createPipes()) {
        LOG_ERROR("Failed to create pipes");
        return false;
    }
    
    // Open pipes
    if (!openPipesWithRetry()) {
        LOG_ERROR("Failed to open pipes, maximum retry count exceeded");
        return false;
    }
    
    return true;
}

bool FifoCommImpl::initializeAsClient() {
    LOG_INFO("Attempting to initialize as client...");
    
    // Set client flag
    isServer_ = false;
    
    // Check if pipes exist
    if (!checkPipesExist()) {
        LOG_ERROR("Pipe files do not exist, please ensure server is running");
        return false;
    }
    
    // Open pipes (with retry mechanism)
    if (!openPipesWithRetry()) {
        LOG_ERROR("Failed to open pipes, maximum retry count exceeded");
        return false;
    }
    
    return true;
}

bool FifoCommImpl::checkPipesExist() {
    struct stat st;
    
    // Check inbound pipe
    if (stat(inPipePath_.c_str(), &st) != 0) {
        LOG_WARN("Inbound pipe does not exist: ", inPipePath_);
        return false;
    } else if (!S_ISFIFO(st.st_mode)) {
        LOG_ERROR("Inbound pipe path exists but is not a pipe: ", inPipePath_);
        return false;
    }
    
    // Check outbound pipe
    if (stat(outPipePath_.c_str(), &st) != 0) {
        LOG_WARN("Outbound pipe does not exist: ", outPipePath_);
        return false;
    } else if (!S_ISFIFO(st.st_mode)) {
        LOG_ERROR("Outbound pipe path exists but is not a pipe: ", outPipePath_);
        return false;
    }
    
    return true;
}

bool FifoCommImpl::createPipes() {
    // Ensure directory exists
    std::string dirPath = basePath_.substr(0, basePath_.find_last_of('/'));
    if (!dirPath.empty()) {
        struct stat st;
        if (stat(dirPath.c_str(), &st) != 0) {
            // Directory doesn't exist, try to create it
            LOG_INFO("Attempting to create directory: ", dirPath);
            if (mkdir(dirPath.c_str(), 0777) != 0) {
                LOG_ERROR("Failed to create directory: ", strerror(errno));
                return false;
            }
        }
    }
    
    // Delete old pipes if they exist
    unlink(inPipePath_.c_str());
    unlink(outPipePath_.c_str());
    
    // Create inbound pipe
    if (mkfifo(inPipePath_.c_str(), 0666) == -1) {
        if (errno != EEXIST) {
            LOG_ERROR("Failed to create inbound pipe: ", strerror(errno));
            return false;
        }
    }
    
    // Create outbound pipe
    if (mkfifo(outPipePath_.c_str(), 0666) == -1) {
        if (errno != EEXIST) {
            LOG_ERROR("Failed to create outbound pipe: ", strerror(errno));
            unlink(inPipePath_.c_str());
            return false;
        }
    }
    
    // Modify permissions to ensure all users can read/write
    chmod(inPipePath_.c_str(), 0666);
    chmod(outPipePath_.c_str(), 0666);
    
    LOG_INFO("Pipes created successfully");
    return true;
}

bool FifoCommImpl::openPipes() {
    if (isServer_) {
        // Server mode - modified pipe opening order: first open inbound pipe in blocking mode, then outbound pipe
        LOG_INFO("Server: Opening inbound pipe in blocking mode");
        // Use blocking mode to wait for client connection
        readFd_ = open(inPipePath_.c_str(), O_RDONLY);
        if (readFd_ == -1) {
            LOG_ERROR("Server: Failed to open inbound pipe: ", strerror(errno));
            return false;
        }
        
        // Set non-blocking mode
        int flags = fcntl(readFd_, F_GETFL, 0);
        fcntl(readFd_, F_SETFL, flags | O_NONBLOCK);
        
        LOG_INFO("Server: Opening outbound pipe");
        writeFd_ = open(outPipePath_.c_str(), O_WRONLY | O_NONBLOCK);
        if (writeFd_ == -1) {
            LOG_ERROR("Server: Failed to open outbound pipe: ", strerror(errno));
            close(readFd_);
            readFd_ = -1;
            return false;
        }
    } else {
        // Client mode - modified pipe opening order: first open outbound pipe in blocking mode, then inbound pipe
        LOG_INFO("Client: Opening outbound pipe in blocking mode");
        // Use blocking mode to wait for server connection
        writeFd_ = open(inPipePath_.c_str(), O_WRONLY); // Client's outbound is server's inbound
        if (writeFd_ == -1) {
            LOG_ERROR("Client: Failed to open outbound pipe: ", strerror(errno));
            return false;
        }
        
        LOG_INFO("Client: Opening inbound pipe");
        readFd_ = open(outPipePath_.c_str(), O_RDONLY | O_NONBLOCK); // Client's inbound is server's outbound
        if (readFd_ == -1) {
            LOG_ERROR("Client: Failed to open inbound pipe: ", strerror(errno));
            close(writeFd_);
            writeFd_ = -1;
            return false;
        }
    }
    
    LOG_INFO("Pipes opened successfully");
    // Notify successful connection
    isConnected_ = true;
    return true;
}

bool FifoCommImpl::openPipesWithRetry() {
    const int MAX_RETRIES = 5; // Reduced retry count
    const int RETRY_DELAY_MS = 1000; // Reduced retry delay
    
    for (int retry = 0; retry < MAX_RETRIES; retry++) {
        if (retry > 0) {
            LOG_INFO("Attempting to open pipes, retry #", retry);
            std::this_thread::sleep_for(std::chrono::milliseconds(RETRY_DELAY_MS));
        }
        
        // Try to open pipes
        if (openPipes()) {
            if (retry > 0) {
                LOG_INFO("Successfully opened pipes after ", retry + 1, " attempts");
            }
            return true;
        }
        
        // Close previously partially opened file descriptors
        if (readFd_ != -1) {
            close(readFd_);
            readFd_ = -1;
        }
        
        if (writeFd_ != -1) {
            close(writeFd_);
            writeFd_ = -1;
        }
    }
    
    return false;
}

void FifoCommImpl::cleanup() {
    // Close file descriptors
    if (readFd_ != -1) {
        close(readFd_);
        readFd_ = -1;
    }
    
    if (writeFd_ != -1) {
        close(writeFd_);
        writeFd_ = -1;
    }
    
    // If server, delete pipe files
    if (isServer_) {
        LOG_INFO("Server: Deleting pipe files");
        unlink(inPipePath_.c_str());
        unlink(outPipePath_.c_str());
    }
    
    // Update connection status
    isConnected_ = false;
    
    LOG_INFO("Pipe cleanup completed");
}

bool FifoCommImpl::sendMessage(const std::string& message) {
    if (writeFd_ == -1) {
        LOG_ERROR("Cannot send message: Write pipe not opened");
        return false;
    }
    
    std::string data = message + "\n";  // Add separator for easier parsing by receiver
    
    // Send message
    ssize_t bytesWritten = write(writeFd_, data.c_str(), data.size());
    if (bytesWritten != static_cast<ssize_t>(data.size())) {
        if (bytesWritten == -1) {
            LOG_ERROR("Failed to write to pipe: ", strerror(errno));
            
            // Check if due to pipe disconnection
            if (errno == EPIPE) {
                LOG_ERROR("Pipe disconnected, receiver may have closed");
            }
        } else {
            LOG_ERROR("Incomplete write to pipe: ", bytesWritten, "/", data.size());
        }
        return false;
    }
    
    return true;
}

bool FifoCommImpl::receiveMessage(std::string& message) {
    if (readFd_ == -1) {
        LOG_ERROR("Cannot receive message: Read pipe not opened");
        return false;
    }
    
    // Check if there is enough data to process
    if (!partialData_.empty()) {
        // Find a complete message
        size_t pos = partialData_.find('\n');
        if (pos != std::string::npos) {
            // Extract a complete message
            message = partialData_.substr(0, pos);
            partialData_ = partialData_.substr(pos + 1);
            
            // If there is still a lot of data left to process, log it
            if (partialData_.size() > 200) {
                LOG_WARN("FIFO receive buffer has a lot of data to process: ", partialData_.size(), " bytes");
            }
            
            return true;
        }
    }
    
    // Read data from pipe
    char buffer[4096];
    ssize_t bytesRead = read(readFd_, buffer, sizeof(buffer) - 1);
    
    if (bytesRead > 0) {
        // Ensure string ends
        buffer[bytesRead] = '\0';
        
        // Add to partial data
        partialData_ += buffer;
        
        // Loop to extract all complete messages until no complete message is found
        size_t pos = partialData_.find('\n');
        if (pos != std::string::npos) {
            // Extract a complete message
            message = partialData_.substr(0, pos);
            partialData_ = partialData_.substr(pos + 1);
            
            // Record remaining data to process
            if (!partialData_.empty()) {
                LOG_DEBUG("FIFO receive processed one message, ", partialData_.size(), " bytes of data left to process");
            }
            
            return true;
        }
    }
    else if (bytesRead == -1) {
        // In non-blocking mode, no data to read returns -1 and EAGAIN
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            LOG_ERROR("Read from pipe failed: ", strerror(errno));
        }
    }
    
    return false;
}

void FifoCommImpl::setReceiveTimeout(int milliseconds) {
    // FIFO implementation does not support timeout setting, use non-blocking mode
    (void)milliseconds;
}

bool FifoCommImpl::isConnected() const {
    return isConnected_;
}

bool FifoCommImpl::isServer() const {
    return isServer_;
} 