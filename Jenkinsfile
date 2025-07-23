pipeline {
    agent any

    stages {
        stage('Build Docker Image') {
            steps {
                script {
                    echo 'Building the Docker image...'
                    // Build the Docker image from the Dockerfile in the current directory
                    // and tag it as 'lush-arch-test'
                    sh 'docker build -t lush-arch-test .'
                }
            }
        }
        stage('Run Tests in Container') {
            steps {
                script {
                    echo 'Running tests inside the Docker container...'
                    // Run the container with the 'lush-arch-test' image.
                    // The CMD in the Dockerfile will be executed.
                    // --rm automatically removes the container when it exits.
                    sh 'docker run --rm lush-arch-test'
                }
            }
        }
        stage('Run Owner-Provided Test Script') {
            steps {
                script {
                    echo 'Running the Lua 5.2 compatibility test script...'
                    // First, create the test script file
                    sh '''
                    cat <<'EOF' > test_52.lua
                    -- Lua 5.2-specific features
                    local _ENV = { print = print, x = 123 } -- lexical environments
                    function show_x()
                        print("x =", x)
                    end
                    show_x()
                    -- Bitwise ops (added in 5.2)
                    local a, b = 0x5, 0x3
                    print("Bitwise AND:", a & b)
                    -- load() replaces loadstring() (binary and text)
                    local f = load("return 10 + 20")
                    print("Loaded result:", f())
                    -- table.pack / table.unpack
                    local t = table.pack(1, 2, 3, nil, 5)
                    print("Packed length:", t.n)
                    print("Unpacked values:", table.unpack(t))
                    EOF
                    '''
                    // Run the container and execute the owner's test script
                    sh 'docker run --rm -v $(pwd)/test_52.lua:/app/test_52.lua lush-arch-test ./bin/Debug/lush/lush test_52.lua'
                }
            }
        }
    }
    post {
        always {
            echo 'Pipeline finished.'
            // Clean up the created docker image to save space
            sh 'docker rmi lush-arch-test || true'
        }
    }
}